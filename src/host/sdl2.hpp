//  sdl2.hpp
//  E64
//
//  Copyright © 2017-2022 elmerucr. All rights reserved.

#ifndef SDL2_HPP
#define SDL2_HPP

namespace E64
{

enum events_output_state {
	QUIT_EVENT = -1,
	NO_EVENT = 0,
	KEYPRESS_EVENT = 1
};

// general init and cleanup
void sdl2_init();
void sdl2_cleanup();

// key states
extern uint8_t *sdl2_keys_last_known_state;

// event related
enum events_output_state sdl2_process_events();
void sdl2_wait_until_enter_released();
void sdl2_wait_until_f_released();
void sdl2_wait_until_q_released();
void sdl2_wait_until_f9_released();
void sdl2_wait_until_r_released();
void sdl2_wait_until_s_released();

// audio related
void		sdl2_start_audio();
void		sdl2_stop_audio();
void		sdl2_queue_audio(void *buffer, unsigned size);
unsigned int	sdl2_get_queued_audio_size_bytes();
double 		sdl2_bytes_per_ms();
uint8_t		sdl2_bytes_per_sample();

}

#endif
