/*
 * mmu.cpp
 * E64
 *
 * Copyright © 2019-2022 elmerucr. All rights reserved.
 */

#include "mmu.hpp"
#include "common.hpp"
#include "rom.hpp"

void E64::mmu_ic::reset()
{
	blit_registers_banked_in = true;
	rom_banked_in = true;
	
	// if available, update rom image
	update_rom_image();
}

uint8_t E64::mmu_ic::read_memory_8(uint16_t address)
{
	uint16_t page = address >> 8;
	
	/*
	 * Mirror first 8 bytes in memory to first 8 bytes from current rom.
	 * Respectively inital SSP and PC.
	 */
	if (!(address & 0xfffffff8)) {
		return current_rom_image[address & 0x7];
	}
	
	if ((page & 0b11111000) == 0b00001000) {
		switch (page) {
			// $0800 - $0fff io range ALWAYS visible
			case IO_BLIT:
				return machine.blitter->io_read_8(address & 0xff);
			case IO_SOUND_PAGE:
				return machine.sound->read_byte(address & 0x1ff);
			case IO_MIXER_PAGE:
				return machine.sound->read_byte(address & 0x1ff);
			case IO_TIMER_PAGE:
				return machine.timer->io_read_byte(address & 0xff);
			case IO_CIA_PAGE:
				return machine.cia->io_read_byte(address & 0xff);
			case IO_SN74LS612:
				return machine.SN74LS612->read_byte(address & 0xff);
			default:
				return machine.blitter->video_memory_read_8(machine.SN74LS612->logical_to_physical(address & 0xffff));
		}
	} else if (((page & 0b11100000) == 0b11000000) && blit_registers_banked_in) {
		// $c000 - $dfff io blit registers (2 x 4 = 8kb)
		// for now:
		return machine.blitter->io_blit_contexts_read_8(address);
	} else if (((page & 0b11100000) == 0b11100000) && rom_banked_in) {
		// $e000 - $ffff rom
		return current_rom_image[address & 0x1fff];
	} else {
		// now it's ram
		return machine.blitter->video_memory_read_8(machine.SN74LS612->logical_to_physical(address & 0xffff));
	}
}

void E64::mmu_ic::write_memory_8(uint16_t address, uint8_t value)
{
	uint16_t page = address >> 8;
	
	if ((page & 0b11111000) == 0b00001000) {
		switch (page) {
			// $0800 - $0fff io range will ALWAYS be written to
			case IO_BLIT:
				machine.blitter->io_write_8(address & 0xff, value);
				break;
			case IO_SOUND_PAGE:
				machine.sound->write_byte(address & 0x1ff, value & 0xff);
				break;
			case IO_MIXER_PAGE:
				machine.sound->write_byte(address & 0x1ff, value & 0xff);
				break;
			case IO_TIMER_PAGE:
				machine.timer->io_write_byte(address & 0xff, value & 0xff);
				break;
			case IO_CIA_PAGE:
				machine.cia->io_write_byte(address & 0xff, value & 0xff);
				break;
			case IO_SN74LS612:
				machine.SN74LS612->write_byte(address &0xff, value & 0xff);
				break;
			default:
				// use ram
				machine.blitter->video_memory_write_8(machine.SN74LS612->logical_to_physical(address & 0xffff), value & 0xff);
				break;
		}
	} else if (((page & 0b11100000) == 0b11000000) && blit_registers_banked_in) {
		// $c000 - $dfff io blit registers (2 x 4 = 8kb)
		machine.blitter->io_blit_contexts_write_8(address, value);
	} else {
		// now it's ram
		machine.blitter->video_memory_write_8(machine.SN74LS612->logical_to_physical(address & 0xffff), value & 0xff);
	}
}

void E64::mmu_ic::update_rom_image()
{
	FILE *f = fopen(host.settings->rom_path, "r");
	
	if (f) {
		printf("[MMU] Found 'rom.bin' in %s, using this image\n",
		       host.settings->settings_dir);
		fread(current_rom_image, 8192, 1, f);
		fclose(f);
	} else {
		printf("[MMU] No 'rom.bin' in %s, using built-in rom\n",
		       host.settings->settings_dir);
		for(int i=0; i<8192; i++) current_rom_image[i] = rom[i];
	}
}

bool E64::mmu_ic::insert_binary(char *file)
{
	uint16_t start_address;
	uint16_t end_address;
	//uint16_t vector;
	
	//const uint8_t magic_code[4] = { 'e'+0x80, '6'+0x80, '4'+0x80, 'x'+0x80 };
	
	FILE *f = fopen(file, "rb");
	uint8_t byte;
	
	if (f) {
		start_address = fgetc(f) << 8;
		start_address |= fgetc(f);
		//vector = fgetc(f) << 8;
		//vector |= fgetc(f);
		
		end_address = start_address;
		
		while(end_address) {
			byte = fgetc(f);
			if( feof(f) ) {
				break;
			}
			write_memory_8(end_address++, byte);
		}
		fclose(f);
		hud.show_notification("%s\n\n"
				      "loading $%04x bytes from $%04x to $%04x",
				      file,
				      end_address - start_address,
				      start_address,
				      end_address);
		printf("[MMU] %s\n"
		       "[MMU] Loading $%04x bytes from $%04x to $%04x\n",
		       file,
		       end_address - start_address,
		       start_address,
		       end_address);
		
		// also update some ram vectors of guest os
		write_memory_8(OS_FILE_START_ADDRESS, start_address >> 8);
		write_memory_8(OS_FILE_START_ADDRESS+1, start_address & 0xff);
		write_memory_8(OS_FILE_END_ADDRESS, end_address >> 8);
		write_memory_8(OS_FILE_END_ADDRESS+1, end_address & 0xff);

		return true;
	} else {
		hud.blitter->terminal_printf(hud.terminal->number, "[MMU] Error: can't open %s\n", file);
		return false;
	}
}
