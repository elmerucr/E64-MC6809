/*
 * SN74LS612.hpp
 * E64
 *
 * Copyright © 2019-2022 elmerucr. All rights reserved.
 */

#include <cstdint>

#ifndef SN74LS612_HPP
#define SN74LS612_HPP

namespace E64
{

class SN74LS612_t {
private:
	/*
	 * Internal 12bit mem registers of mmu (only bits 12-23 used)
	 */
	uint32_t registers[16];
public:
	/*
	 * Each register contains a 12bit pointer to a 4kb block
	 * in 16mb of ram (as seen from the mmu). After a reset,
	 * see first 64k of ram
	 */
	void reset()
	{
		for(int i=0; i<16; i++) registers[i] = i << 12;
	}

	
	uint8_t read_byte(uint8_t address)
	{
		if (address & 0b1) {
			// low byte
			return (registers[(address & 0x1f) >> 1] & 0xf000) >> 8;
		} else {
			// high byte
			return (registers[(address & 0x1f) >> 1] & 0xff0000) >> 16;
		}
	}
	
	void write_byte(uint8_t address, uint8_t byte)
	{
		if (address & 0b1) {
			// low byte
			registers[(address & 0x1f) >> 1] =
				(registers[(address & 0x1f) >> 1] & 0x00ff0000) |
				((byte & 0xf0) << 8);
		} else {
			// high byte
			registers[(address & 0x1f) >> 1] =
				(registers[(address & 0x1f) >> 1] & 0x0000f000) |
				(byte << 16);
	    }
	}
	
	inline uint32_t logical_to_physical(uint16_t address)
	{
		return registers[address >> 12] | (address & 0x0fff);
	}
};

}

#endif
