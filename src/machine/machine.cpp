/*
 * machine.cpp
 * E64
 *
 * Copyright © 2019-2022 elmerucr. All rights reserved.
 */

#include "machine.hpp"
#include "sdl2.hpp"
#include "common.hpp"

#include <cmath>
#include <unistd.h>

#define MACHINE_SR	0x00
#define MACHINE_CR	0x01

static int pokeb(lua_State *L)
{
	uint16_t address = lua_tonumber(L, 1);
	uint8_t byte = lua_tonumber(L, 2);
	machine.mmu->write_memory_8(address, byte);
	return 0;	// no of results
}

/*
 * mmu glue function needed for MC6809 constructor
 */
inline uint8_t read8(uint16_t address)
{
	return machine.mmu->read_memory_8(address);
}

/*
 * mmu glue function needed for MC6809 constructor
 */
inline void write8(uint16_t address, uint8_t byte)
{
	machine.mmu->write_memory_8(address, byte);
}

E64::machine_t::machine_t()
{
	underruns = equalruns = overruns = 1;
	under_lap = equal_lap = over_lap = 1;
	
	mmu = new mmu_ic();
	
	SN74LS612 = new SN74LS612_t();
	
	exceptions = new exceptions_ic();
	
	cpu = new mc6809(read8, write8);
	cpu->assign_nmi_line(&exceptions->nmi_output_pin);
	cpu->assign_irq_line(&exceptions->irq_output_pin);
	
	m68k = new m68k_ic();
	
	timer = new timer_ic(exceptions);
	
	blitter = new blitter_ic();
	blitter->connect_exceptions_ic(exceptions);
	
	sound = new sound_ic();
	
	cia = new cia_ic();
	
	/*
	 * Init clocks (frequency dividers)
	 */
	cpu_to_sid = new clocks(CPU_CLOCK_SPEED, SID_CLOCK_SPEED);
	
	recording_sound = false;
	
	/*
	 * Lua, init with nullpointer
	 */
	L = nullptr;
	lua_initialized = false;
	
	lua_dir[0] = '\0';
	lua_dir_assets[0] = '\0';
	lua_set_dir(host.settings->game_dir_at_init);
}

E64::machine_t::~machine_t()
{
	uint64_t total = underruns + equalruns + overruns;
	
	printf("[Machine] Audiobuffer overall performance:\n"
	       "[Machine]  underruns:     %6.2f%%\n"
	       "[Machine]  equalruns:     %6.2f%%\n"
	       "[Machine]  overruns:      %6.2f%%\n",
	       (double)underruns*100/total,
	       (double)equalruns*100/total,
	       (double)overruns*100/total);
	
	if (recording_sound) {
		stop_recording_sound();
	}
	
	if (L) {
		printf("[Machine] Closing Lua\n");
		lua_close(L);
	}
	
	delete cpu_to_sid;
	delete cia;
	delete sound;
	delete blitter;
	delete timer;
	delete m68k;
	delete cpu;
	delete exceptions;
	delete SN74LS612;
	delete mmu;
}

bool E64::machine_t::run(uint16_t cycles)
{
	cpu_cycle_saldo += cycles;
	m68k_cycle_saldo += cycles;
	
	/*
	 * A fine grained cycles_step is needed to be able run the
	 * proper amount of cycles on both cia and timer after each
	 * cpu instruction. That way, there'll be a decent emulation
	 * of interrupt triggering and acknowledgements.
	 *
	 * Note: This implies that cia and timer run at the same clock
	 * speed as the cpu.
	 */
	uint8_t cycles_step;
	
	int32_t consumed_cycles = 0;
	
	do {
		m68k->execute();
		cycles_step = m68k->getClock();
		cycles_step = cpu->execute();
		cia->run(cycles_step);
		timer->run(cycles_step);
		consumed_cycles += cycles_step;
	} while ((!cpu->breakpoint()) && (consumed_cycles < cpu_cycle_saldo));
	
	/*
	 * After reaching a breakpoint, it can be expected that the full
	 * amount of desired cycles wasn't completed yet. To make sure
	 * the step function in debugger mode works correctly, empty any
	 * remaining desired cycles by putting the cycle_saldo on 0.
	 */
	if (cpu->breakpoint()) {
		cpu_cycle_saldo = 0;
	} else {
		cpu_cycle_saldo -= consumed_cycles;
	}

	/*
	 * Run cycles on sound device & start audio if buffer is large
	 * enough. The aim is to have as much synchronization between
	 * CPU, timers and sound as possible. That way, music and other
	 * sound effects will sound as regularly as possible.
	 * If buffer size deviates too much, an adjusted amount of cycles
	 * will be run on sound.
	 */
	unsigned int audio_queue_size = stats.current_audio_queue_size();

	if (!recording_sound) {
		/* not recording sound */
		if (audio_queue_size < (0.5 * AUDIO_BUFFER_SIZE)) {
			sound->run(cpu_to_sid->clock(1.05 * consumed_cycles));
			underruns++;
		} else if (audio_queue_size < 1.2 * AUDIO_BUFFER_SIZE) {
			sound->run(cpu_to_sid->clock(consumed_cycles));
			equalruns++;
		} else if (audio_queue_size < 2.0 * AUDIO_BUFFER_SIZE) {
			sound->run(cpu_to_sid->clock(0.95 * consumed_cycles));
			overruns++;
		} else overruns++;
	} else {
		/* recording sound */
		if (audio_queue_size < (0.5 * AUDIO_BUFFER_SIZE)) {
			underruns++;
		} else if (audio_queue_size < 1.2 * AUDIO_BUFFER_SIZE) {
			equalruns++;
		} else {
			overruns++;
		}
		
		sound->run(cpu_to_sid->clock(consumed_cycles));
		
		float sample;
		
		while (machine.sound->record_buffer_pop(&sample)) {
			host.settings->write_to_wav(sample);
		}
	}
	
	if (audio_queue_size > (3*AUDIO_BUFFER_SIZE/4))
		E64::sdl2_start_audio();
	
	frame_cycle_saldo += consumed_cycles;
	
	if (frame_cycle_saldo > CPU_CYCLES_PER_FRAME) {
		frame_is_done = true;
		
		/*
		 * Warn blitter for possible IRQ pull
		 */
		blitter->notify_screen_refreshed();
		
		
		frame_cycle_saldo -= CPU_CYCLES_PER_FRAME;
		
		/*
		 * run Lua update, might add some more blits
		 */
		lua_update();
		
		/*
		 * Then run blitter
		 */
		blitter->run(BLIT_CYCLES_PER_FRAME);
	}
	
	return cpu->breakpoint();
}

void E64::machine_t::reset()
{
	printf("[Machine] System reset\n");
	
	cpu_cycle_saldo = 0;
	m68k_cycle_saldo = 0;
	frame_cycle_saldo = 0;
	frame_is_done = false;
	
	mmu->reset();
	SN74LS612->reset();
	sound->reset();
	blitter->reset();
	timer->reset();
	cia->reset();
	cpu->reset();
	m68k->reset();
	
	lua_initialized = false;
}

void E64::machine_t::toggle_recording_sound()
{
	if (!recording_sound) {
		start_recording_sound();
	} else {
		stop_recording_sound();
	}
}

void E64::machine_t::start_recording_sound()
{
	recording_sound = true;
	sound->clear_record_buffer();
	host.settings->create_wav();
	hud.show_notification("start recording sound");
}

void E64::machine_t::stop_recording_sound()
{
	recording_sound = false;
	host.settings->finish_wav();
	hud.show_notification("stop recording sound");
}

bool E64::machine_t::buffer_within_specs()
{
	bool result = !((underruns > under_lap) || (overruns > over_lap));
	
	under_lap = underruns;
	equal_lap = equalruns;
	over_lap  = overruns ;
	
	return result;
}

void E64::machine_t::flip_modes()
{
	if (mode == RUNNING) {
		mode = PAUSED;
	} else {
		mode = RUNNING;
	}
	
	host.video->update_title();
}

bool E64::machine_t::lua_init()
{
	/*
	 * Start LUA virtual machine (first close it, if one is running)
	 */
	if (L) {
		printf("[Machine] Closing Lua\n");
		lua_close(L);
	}
	L = luaL_newstate();
	luaL_openlibs(L);
	if (L) {
		printf("[Machine] Starting %s\n", LUA_COPYRIGHT);
	} else {
		return false;
	}
	
	lua_pushcfunction(L, pokeb);
	lua_setglobal(L, "pokeb");
	
	if (luaL_dofile(L, "main.lua") == LUA_OK) {
		lua_getglobal(L, "init");
		if (lua_pcall(L, 0, 0, 0)) {
			hud.blitter->terminal_printf(hud.terminal->number, "Lua Error: %s\n", lua_tostring(L, -1));
			lua_pop(L, 1);
			flip_modes();
			return false;
		}
	} else {
		hud.blitter->terminal_printf(hud.terminal->number, "Lua Error: %s\n", lua_tostring(L, -1));
		lua_pop(L, 1);
		flip_modes();
		return false;
	}
	
	return true;
}

void E64::machine_t::lua_update()
{
	if (lua_initialized) {
		lua_getglobal(L, "update");
		if (lua_pcall(L, 0, 0, 0)) {
			hud.blitter->terminal_printf(hud.terminal->number, "Lua Error: %s\n", lua_tostring(L, -1));
			lua_pop(L, 1);
			flip_modes();
		}
	} else {
		lua_initialized = lua_init();
	}
}

void E64::machine_t::lua_set_dir(const char *path)
{
	if (chdir(path)) {
		/*
		 * Path doesn't exist
		 */
	} else {
		printf("[Machine] Game directory set to:\n%s\n", path);
		hud.show_notification("Game directory set to:\n%s", path);
		strcpy(lua_dir, path);
		sprintf(lua_dir_assets, "%s/assets", lua_dir);
		chdir(lua_dir);
//		if (chdir(game_assets_dir)) {
//			printf("[Settings] Error: In %s no assets directory found\n", game_dir);
//			game_assets_dir[0] = '\0';
//			return false;
//		} else {
//			printf("[Settings] %s is now the assets directory\n", game_assets_dir);
//			/*
//			 * And switch back to gamedir
//			 */
//			chdir(game_dir);
//		}
	}
}
