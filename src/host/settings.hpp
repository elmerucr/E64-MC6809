/*
 * settings.hpp
 * E64
 *
 * Copyright © 2021-2022 elmerucr. All rights reserved.
 */
 
#ifndef SETTINGS_HPP
#define SETTINGS_HPP

#include <cstdio>
#include <cstdint>

#include "lua.hpp"

namespace E64 {

struct wav_header_t {
	/*
	 * The "RIFF" chunk descriptor (12 bytes)
	 */
	uint8_t	ChunkID[4];		// "RIFF"
	uint8_t	ChunkSize[4];
	uint8_t	Format[4];		// "WAVE"
	
	/*
	 * The "fmt" sub-chunk (24 bytes)
	 */
	uint8_t	Subchunk1ID[4];		// "fmt "
	uint8_t	Subchunk1Size[4];	// 16
	uint8_t AudioFormat[2];
	uint8_t NumChannels[2];
	uint8_t SampleRate[4];
	uint8_t ByteRate[4];
	uint8_t BlockAlign[2];
	uint8_t BitsPerSample[2];
	
	/*
	 * The "data" subchunk
	 */
	uint8_t Subchunk2ID[4];		// "data"
	uint8_t Subchunk2Size[4];
};

class settings_t
{
private:
	FILE *wav_file;

	void write_settings();
	
	struct wav_header_t wav_header;
	
	/*
	 * Lua virtual machine for reading settings file settings.lua
	 */
	lua_State *L;
public:
	settings_t();
	~settings_t();

	char home_dir[256];
	char settings_dir[256];
	char rom_path[256];
	
	bool fullscreen_at_init;
	char game_dir_at_init[256];
	bool scanlines_at_init;
	
	bool create_wav();
	
	inline void write_to_wav(float sample)
	{
	//	// enforce little endian
	//	uint8_t lbyte = ((uint16_t)sample) & 0xff;
	//	uint8_t hbyte = ((uint16_t)sample) >> 8;
	//	fwrite((void *)&lbyte, 1, 1, wav_file);
	//	fwrite((void *)&hbyte, 1, 1, wav_file);
		
		fwrite((void *)&sample, 4, 1, wav_file);
	}
	
	void finish_wav();
};

}

#endif
