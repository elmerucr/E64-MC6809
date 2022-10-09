/*
 * sound.hpp
 * E64
 *
 * Copyright © 2019-2022 elmerucr. All rights reserved.
 */

#include <cstdio>
#include <cstdint>

// resid header
#include "sid.h"
#include "analog.hpp"

#ifndef SOUND_HPP
#define SOUND_HPP

namespace E64
{

class sound_ic {
private:
	/*
	 * sid variables etc...
	 */
	SID sid[4];
	cycle_count delta_t_sid0;
	int16_t sample_buffer_mono_sid0[65536];
	cycle_count delta_t_sid1;
	int16_t sample_buffer_mono_sid1[65536];
	cycle_count delta_t_sid2;
	int16_t sample_buffer_mono_sid2[65536];
	cycle_count delta_t_sid3;
	int16_t sample_buffer_mono_sid3[65536];
	
	/*
	 * Used to rewire several lo/hi registers from sid to big endian
	 * and make it more convenient.
	 */
	uint8_t register_index[32];

	/*
	 * Analog
	 */
	analog_ic analog0;
	analog_ic analog1;
	analog_ic analog2;
	analog_ic analog3;
	int16_t sample_buffer_mono_analog0[65536];
	int16_t sample_buffer_mono_analog1[65536];
	int16_t sample_buffer_mono_analog2[65536];
	int16_t sample_buffer_mono_analog3[65536];
	
	/*
	 * General
	 */
	uint8_t balance_registers[0x10];
	float sample_buffer_stereo[131072];
	
	float record_buffer[65536];
	uint16_t record_buffer_head;
	uint16_t record_buffer_tail;
public:
	sound_ic();
	~sound_ic();
	// read and write functions to data registers of sid array and mixer
	uint8_t read_byte(uint16_t address);
	void write_byte(uint16_t address, uint8_t byte);
	// run the no of cycles that need to be processed by the sid chips on the sound device
	// and process all the accumulated cycles (flush into soundbuffer)
	void run(uint32_t number_of_cycles);
	void reset();
	
	
	
	void clear_record_buffer();
	
	inline void record_buffer_push(float sample)
	{
		record_buffer[record_buffer_head] = sample;
		record_buffer_head++;
	}
	
	inline bool record_buffer_pop(float *sample)
	{
		if (record_buffer_head == record_buffer_tail) {
			return false;
		} else {
			*sample = record_buffer[record_buffer_tail];
			record_buffer_tail++;
			return true;
		}
	}
};

}

#endif