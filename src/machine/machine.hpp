/*
 * machine.hpp
 * E64
 *
 * Copyright © 2019-2022 elmerucr. All rights reserved.
 */

#ifndef MACHINE_HPP
#define MACHINE_HPP

#include "cia.hpp"
#include "clocks.hpp"
#include "mmu.hpp"
#include "SN74LS612.hpp"
#include "sound.hpp"
#include "timer.hpp"
#include "blitter.hpp"
#include "mc6809.hpp"
#include "m68k.hpp"
#include "exceptions.hpp"
#include "lua.hpp"

namespace E64
{

enum mode_t {
	RUNNING,
	PAUSED
};

class machine_t {
private:	
	clocks *cpu_to_sid;
	char machine_help_string[2048];
	int32_t cpu_cycle_saldo;
	int32_t m68k_cycle_saldo;
	int32_t frame_cycle_saldo;
	bool frame_is_done;
	
	/*
	 * Keeping track of soundbuffer and its performance
	 */
	uint64_t underruns, equalruns, overruns;
	uint64_t under_lap, equal_lap, over_lap;
	
	bool recording_sound;
	
	void start_recording_sound();
	void stop_recording_sound();
public:
	enum mode_t mode;

	mmu_ic		*mmu;
	SN74LS612_t	*SN74LS612;
	exceptions_ic	*exceptions;
	mc6809		*cpu;
	m68k_ic		*m68k;
	timer_ic	*timer;
	blitter_ic	*blitter;
	sound_ic	*sound;
	cia_ic		*cia;

	machine_t();
	~machine_t();

	bool run(uint16_t no_of_cycles);

	void reset();
	
	void flip_modes();
	
	inline bool frame_done() {
		bool result = frame_is_done;
		if (frame_is_done)
			frame_is_done = false;
		return result;
	}
	
	inline int32_t frame_cycles() { return frame_cycle_saldo; }
	
	/*
	 * Sound related
	 */
	void toggle_recording_sound();
	inline bool recording() { return recording_sound; }
	bool buffer_within_specs();
	
	/*
	 * LUA virtual machine
	 */
	lua_State *L;
	bool lua_initialized;
	char lua_dir[256];
	char lua_dir_assets[256];
	void lua_set_dir(const char *path);
	bool lua_init();
	void lua_update();
};

}

#endif
