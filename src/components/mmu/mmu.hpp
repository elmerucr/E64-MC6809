/*
 * mmu.hpp
 * E64
 *
 * Copyright © 2019-2022 elmerucr. All rights reserved.
*/

#ifndef MMU_HPP
#define MMU_HPP

#include <cstdint>
#include <cstdlib>

#define IO_BLIT		0x0008
#define IO_TIMER_PAGE	0x000b
#define IO_SOUND_PAGE	0x000c
#define IO_MIXER_PAGE	0x000d
#define IO_CIA_PAGE	0x000e
#define IO_SN74LS612	0x000f

namespace E64
{

class mmu_ic {
public:
	void reset();
	
	bool blit_registers_banked_in;
	bool rom_banked_in;
	
	uint8_t read_memory_8(uint16_t address);
	void write_memory_8(uint16_t address, uint8_t value);
	
	uint16_t read_memory_16(uint32_t address);
	void write_memory_16(uint32_t address, uint16_t value);
	
	uint8_t  current_rom_image[8192];
	
	void update_rom_image();
	
	bool insert_binary(char *file);
};

}

#endif
