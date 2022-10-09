/*
 * settings.cpp
 * E64
 *
 * Copyright © 2021-2022 elmerucr. All rights reserved.
 */

#include "settings.hpp"
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <sys/stat.h>
#include <unistd.h>
#include "common.hpp"

E64::settings_t::settings_t()
{
	/*
	 * Within apple app container the next statement returns a long
	 * path.
	 */
	snprintf(settings_dir, 256, "%s", getenv("HOME"));

	/*
	 * Shorten the path name to home and user dir
	 */
	char *iterator = settings_dir;
	int times = 0;
	while (times < 3) {
		if((*iterator == '/') || (*iterator == '\0')) times++;
		iterator++;
	}
	iterator--;
	/*
	 * Force '\0' character to terminate string to copy to 'home_dir'
	 */
	*iterator = '\0';
    
	strcpy(home_dir, settings_dir);
	printf("[Settings] User home directory: %s\n", home_dir);

	/*
	 * Iterator still points to the last char (\0) inside
	 * settings_path, now use it to update the true settings path.
	 */
	snprintf(iterator, 256, "/.E64");

	printf("[Settings] Opening settings directory: %s\n", settings_dir);
	if (chdir(settings_dir)) {
		printf("[Settings] Settings directory doesn't exist, creating it...\n");
		mkdir(settings_dir, 0777);
	}

	snprintf(rom_path, 256, "%s/rom.bin", settings_dir);

	/*
	 * Switch to settings path and read settings file using Lua
	 */
	chdir(settings_dir);

	L = luaL_newstate();
	luaL_openlibs(L);
	
	if (luaL_dofile(L, "settings.lua") == LUA_OK) {
		printf("[Settings] Reading 'settings.lua'\n");
	} else {
		printf("[Settings] settings.lua not found, using default settings\n");
	}
	
//	lua_getglobal(L, "game_dir");
//	if (lua_isstring(L, -1)) {
//		set_game_dir_path(lua_tolstring(L, -1, nullptr));
//	} else {
//		printf("[Settings] game_dir not existing or valid\n");
//		game_dir[0] = '\0';
//	}
	
	lua_getglobal(L, "fullscreen");
	if (lua_isboolean(L, -1)) {
		fullscreen_at_init = lua_toboolean(L, -1);
	} else {
		fullscreen_at_init = false;
	}
	
	lua_getglobal(L, "game_dir");
	if (lua_isstring(L, -1)) {
		strcpy(game_dir_at_init, lua_tolstring(L, -1, nullptr));
	} else {
		game_dir_at_init[0] = '\0';
	}
	
	lua_getglobal(L, "scanlines");
	if (lua_isboolean(L, -1)) {
		scanlines_at_init = lua_toboolean(L, -1);
	} else {
		scanlines_at_init = true;
	}
	
	lua_close(L);
	
	/*
	 * Prepare sound export
	 */
	wav_file = nullptr;
	
	wav_header = {
		{ 0x52, 0x49, 0x46, 0x46 },	// "RIFF"
		{ 0x00, 0x00, 0x00, 0x00 },	// chunksize = needs to be calculated
		{ 0x57, 0x41, 0x56, 0x45 },	// "WAVE"
		
		{ 0x66, 0x6d, 0x74, 0x20 },	// "fmt "
		{ 0x10, 0x00, 0x00, 0x00 },	// 16
		{ 0x03, 0x00             },	// type of format = floating point PCM
		{ 0x02, 0x00             },	// two channels
		{ SAMPLE_RATE&0xff, (SAMPLE_RATE>>8)&0xff, (SAMPLE_RATE>>16)&0xff, (SAMPLE_RATE>>24)&0xff },
		{ (8*SAMPLE_RATE)&0xff, ((8*SAMPLE_RATE)>>8)&0xff, ((8*SAMPLE_RATE)>>16)&0xff, ((8*SAMPLE_RATE)>>24)&0xff },
		{ 0x08, 0x00             },	// block align
		{ 0x20, 0x00             },	// 32 bits per sample
		
		{ 0x64, 0x61, 0x74, 0x61 },	// "data"
		{ 0x00, 0x00, 0x00, 0x00 }	// subchunk2size
	};
}

E64::settings_t::~settings_t()
{
	write_settings();
}

void E64::settings_t::write_settings()
{
	/*
	 * This is currently some kind of hack.
	 */
	chdir(settings_dir);
	FILE *temp_file = fopen("settings.lua", "w");
	
	if(!temp_file) {
		printf("Error: Can't open file 'settings.lua' for writing\n");
	} else {
		fwrite("-- Automatically generated settings file for E64", 1, 48, temp_file);

		if (host.video->is_fullscreen()) {
			fwrite("\nfullscreen = true", 1, 18, temp_file);
		} else {
			fwrite("\nfullscreen = false", 1, 19, temp_file);
		}
		
		fwrite("\ngame_dir = \"", 1, 13, temp_file);
		char *temp_char = machine.lua_dir;
		while (*temp_char != '\0') {
			fwrite(temp_char, 1, 1, temp_file);
			temp_char++;
		}
		fwrite("\"", 1, 1, temp_file);
		
		if (host.video->is_using_scanlines()) {
			fwrite("\nscanlines = true", 1, 17, temp_file);
		} else {
			fwrite("\nscanlines = false", 1, 18, temp_file);
		}
		
		fclose(temp_file);
	}
}

bool E64::settings_t::create_wav()
{
	chdir(settings_dir);
	
	wav_file = fopen("output.wav", "wb");
	
	if (!wav_file) return false;
	
	// write unfinished header to file
	fwrite((void *)&wav_header, 1, 44, wav_file);

	return true;
}

void E64::settings_t::finish_wav()
{
	long filesize;
	
	if (wav_file) {
		fseek(wav_file, 0L, SEEK_END);
		filesize = ftell(wav_file);
		
		// enforce little endian
		uint32_t sc2s = (uint32_t)(filesize - 44);
		wav_header.Subchunk2Size[0] = sc2s         & 0xff;
		wav_header.Subchunk2Size[1] = (sc2s >> 8)  & 0xff;
		wav_header.Subchunk2Size[2] = (sc2s >> 16) & 0xff;
		wav_header.Subchunk2Size[3] = (sc2s >> 24) & 0xff;
		
		// enforce little endian
		uint32_t cs = sc2s + 36;
		wav_header.ChunkSize[0] = cs         & 0xff;
		wav_header.ChunkSize[1] = (cs >> 8)  & 0xff;
		wav_header.ChunkSize[2] = (cs >> 16) & 0xff;
		wav_header.ChunkSize[3] = (cs >> 24) & 0xff;
		
		printf("[Settings] wav chunksize is: %i\n", cs);
		printf("[Settings] wav subchunk2size is: %i\n", sc2s);
		
		// write correct/final header
		fseek(wav_file, 0L, SEEK_SET);
		fwrite((void *)&wav_header, 1, 44, wav_file);
		
		fclose(wav_file);
	}
}
