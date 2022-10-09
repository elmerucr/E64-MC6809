/*
 * blit.hpp
 * E64
 *
 * Copyright © 2022 elmerucr. All rights reserved.
 */

#ifndef BLIT_HPP
#define BLIT_HPP

#define COMMAND_BUFFER_SIZE 63+(3*64)

#include <cstdint>

namespace E64
{

enum terminal_output_type {
	NOTHING,
	ASCII,
	BLITTER
};

/*
 * The next class is a surface blit. It is also used for terminal type
 * operations.
 */

class blit_t {
public:
	uint8_t number;

	/*
	 * Flags 0 (as encoded inside machine)
	 *
	 * 7 6 5 4 3 2 1 0
	 * |       | | |
	 * |       | | |
	 * |       | | +---- Background (0 = off, 1 = on)
	 * |       | +------ Single color (0) / Multi color (1)
	 * |       +-------- Color per tile (0 = off, 1 = on)
	 * +---------------- Pixel data video ram (0) / Or cbm font (1)
	 *
	 * bits 0 and 4-6: Reserved
	 */
	bool background;	// bit 1
	bool multicolor_mode;	// bit 2
	bool color_per_tile;	// bit 3
	bool use_cbm_font;	// bit 7

	/*
	 * Flags 1 - Stretching & Flips (as encoded inside machine)
	 *
	 * 7 6 5 4 3 2 1 0
	 * | | | |     | |
	 * | | | |     | +-- Horizontal stretching (0 = off, 1 = on)
	 * | | | |     +---- Vertical stretching (0 = off, 1 = on)
	 * | | | +---------- Horizontal flip (0 = off, 1 = on)
	 * | | +------------ Vertical flip (0 = off, 1 = on)
	 * +-+-------------- Rotate right per 90 degrees (0, 1, 2, 3)
	 *
	 * bits 1, 3, 6 and 7: Reserved
	 */
	uint8_t flags_1;
	/*
	 * Internal
	 */
	bool hor_stretch;	// bit 0
	bool ver_stretch;	// bit 2
	bool hor_flip;		// bit 4
	bool ver_flip;		// bit 5
	bool rotate;		// bit 6

	/*
	 * Size of blit in pixels log2, 8 bit unsigned number.
	 *
	 * 7 6 5 4 3 2 1 0
	 * | | | | | | | |
	 * | | | | +-+-+-+-- Width
	 * | | | |
	 * +-+-+-+---------- Height
	 *
	 * Low nibble codes for width (in pixels log2) of the blit.
	 * High nibble codes for height.
	 *
	 * The bits of each nibble indicate a number of 2 - 9 (n).
	 * All other nibble values are illegal.
	 *
	 * Min width = 2^2 = 4pix    Min height = 2^2 = 4pix
	 * Max width = 2^9 = 512pix  Max height = 2^9 = 512pix
	 */
	uint8_t size_in_pixels_log2;	// BASIC property
	
	/*
	 * Size of tiles in pixels log2, 8 bit unsigned number.
	 *
	 * 7 6 5 4 3 2 1 0
	 * | | | | | | | |
	 * | | | | +-+-+-+-- Width
	 * | | | |
	 * +-+-+-+---------- Height
	 *
	 * Low nibble codes for width (in pixels log2) of each tile.
	 * High nibble codes for height.
	 *
	 * The bits of each nibble indicate a number of 2 - 9 (n).
	 * All other nibble values are illegal.
	 *
	 * Min width = 2^2 = 4pix    Min height = 2^2 = 4pix
	 * Max width = 2^9 = 512pix  Max height = 2^9 = 512pix
	 */
	uint8_t tile_size_in_pixels_log2;	// BASIC property

	uint8_t  width_in_tiles_log2;
	uint16_t width_log2;            // width of the blit source in log2(pixels)
	uint16_t width;			// width of the blit source in pixels
	uint16_t width_mask;
	uint16_t width_on_screen_log2;  // width of the final bit on screen in pixels (might be doubled) and log2
	uint16_t width_on_screen;	// width of the final bit on screen in pixels (might be doubled)
	uint16_t width_on_screen_mask;
	
	uint16_t tile_width_log2;
	uint16_t tile_width;
	uint16_t tile_width_mask;

	uint8_t  height_in_tiles_log2;
	uint16_t height_log2;		// height of the blit source in log2(pixels)
	uint16_t height;		// height of the blit source in pixels
	uint16_t height_on_screen_log2;	// height of the final bit on screen in pixels (might be doubled) and log2
	uint16_t height_on_screen;	// height of the final bit on screen in pixels (might be doubled)
	
	uint16_t tile_height_log2;
	uint16_t tile_height;
	uint16_t tile_height_mask;

	uint32_t total_no_of_pix;	// total number of pixels to blit onto framebuffer for this blit

	/*
	 * Contains the foreground color (for both single color AND current terminal color)
	 */
	uint16_t foreground_color;

	/*
	 * Contains the background color (for both single color AND current terminal color)
	 */
	uint16_t background_color;

	int16_t x_pos;
	int16_t y_pos;
	
	/*
	 * Allows a max of 2^9 = 512 pixels width, 2^9 = 512 pixels height
	 */
	inline void set_size_in_pixels_log2(uint8_t size)
	{
		uint8_t _width_log2 = size & 0b1111;
		if (_width_log2 < 2) {
			_width_log2 = 2;
		} else if (_width_log2 > 9) {
			_width_log2 = 9;
		}
		
		uint8_t _height_log2 = (size & 0b11110000) >> 4;
		if (_height_log2 < 2) {
			_height_log2 = 2;
		} else if (_height_log2 > 9) {
			_height_log2 = 9;
		}
		
		if ((size & 0b1111) > 6) {
			size = (size & 0b01110000) | 0b110;
		}
		if (((size & 0b1110000) >> 4) > 5) {
			size = (size & 0b00001111) | 0b1010000;
		}

		size_in_pixels_log2 = _width_log2 | (_height_log2 << 4);

		calculate_dimensions();
	}
	
	inline void set_tile_size_in_pixels_log2(uint8_t size)
	{
		uint8_t _width_log2 = size & 0b1111;
		if (_width_log2 < 2) {
			_width_log2 = 2;
		} else if (_width_log2 > 9) {
			_width_log2 = 9;
		}
		
		uint8_t _height_log2 = (size & 0b11110000) >> 4;
		if (_height_log2 < 2) {
			_height_log2 = 2;
		} else if (_height_log2 > 9) {
			_height_log2 = 9;
		}
		
		if ((size & 0b1111) > 6) {
			size = (size & 0b01110000) | 0b110;
		}
		if (((size & 0b1110000) >> 4) > 5) {
			size = (size & 0b00001111) | 0b1010000;
		}

		tile_size_in_pixels_log2 = _width_log2 | (_height_log2 << 4);

		calculate_dimensions();
	}

	inline void calculate_dimensions()
	{
		width_log2 = size_in_pixels_log2 & 0b1111;
		width = 1 << width_log2;
		width_mask = width - 1;
		hor_stretch &= 0b11;
		width_on_screen_log2 = width_log2 + hor_stretch;
		width_on_screen = 1 << width_on_screen_log2;
		width_on_screen_mask = width_on_screen - 1;
		
		tile_width_log2 = tile_size_in_pixels_log2 & 0b1111;
		tile_width = 0b1 << tile_width_log2;
		tile_width_mask = tile_width - 1;
		
		if (width_log2 < tile_width_log2) {
			width_in_tiles_log2 = 0;
		} else {
			width_in_tiles_log2 = width_log2 - tile_width_log2;
		}

		columns = 0b1 << width_in_tiles_log2;
		
		height_log2 = (size_in_pixels_log2 & 0b11110000) >> 4;
		height = 1 << height_log2;
		ver_stretch &= 0b11;
		height_on_screen_log2 = height_log2 + ver_stretch;
		height_on_screen = 1 << height_on_screen_log2;
		
		tile_height_log2 = (tile_size_in_pixels_log2 & 0b11110000) >> 4;
		tile_height = 0b1 << tile_height_log2;
		tile_height_mask = tile_height - 1;
		
		if (height_log2 < tile_height_log2) {
			height_in_tiles_log2 = 0;
		} else {
			height_in_tiles_log2 = height_log2 - tile_height_log2;
		}

		rows = 0b1 << height_in_tiles_log2;

		tiles = columns * rows;
	}
	
	inline void process_flags_1()
	{
		flags_1 &= 0b11110011;
		
		if (flags_1 & 0b00000001) {
			hor_stretch = true;
		} else {
			hor_stretch = false;
		}
		
		if (flags_1 & 0b00000010) {
			ver_stretch = true;
		} else {
			ver_stretch = false;
		}
		
		switch ((flags_1 & 0b11110000) >> 4) {
			case 0b0000:	hor_flip = false; ver_flip = false; rotate = false; break;
			case 0b0001:	hor_flip = true ; ver_flip = false; rotate = false; break;
			case 0b0010:	hor_flip = false; ver_flip = true ; rotate = false; break;
			case 0b0011:	hor_flip = true ; ver_flip = true ; rotate = false; break;
			case 0b0100:	hor_flip = false; ver_flip = false; rotate = true ; break;
			case 0b0101:	hor_flip = true ; ver_flip = false; rotate = true ; break;
			case 0b0110:	hor_flip = false; ver_flip = true ; rotate = true ; break;
			case 0b0111:	hor_flip = true ; ver_flip = true ; rotate = true ; break;
			case 0b1000:	hor_flip = true ; ver_flip = true ; rotate = false; break;
			case 0b1001:	hor_flip = false; ver_flip = true ; rotate = false; break;
			case 0b1010:	hor_flip = true ; ver_flip = false; rotate = false; break;
			case 0b1011:	hor_flip = false; ver_flip = false; rotate = false; break;
			case 0b1100:	hor_flip = true ; ver_flip = true ; rotate = true ; break;
			case 0b1101:	hor_flip = false; ver_flip = true ; rotate = true ; break;
			case 0b1110:	hor_flip = true ; ver_flip = false; rotate = true ; break;
			case 0b1111:	hor_flip = false; ver_flip = false; rotate = true ; break;
		}
	}

	inline uint8_t get_size_in_pixels_log2()
	{
		return size_in_pixels_log2;
	}

	inline uint8_t get_tile_size_in_pixels_log2()
	{
		return tile_size_in_pixels_log2;
	}

	uint8_t  columns;
	uint16_t rows;
	uint16_t tiles;

	uint16_t cursor_position;
	uint8_t  cursor_interval;
	uint8_t  cursor_countdown;
	char     cursor_original_char;
	uint16_t cursor_original_color;
	uint16_t cursor_original_background_color;
	bool     cursor_blinking;
	bool     cursor_big_move;
	char     command_buffer[COMMAND_BUFFER_SIZE];

	inline uint8_t get_columns()
	{
		return columns;
	}

	inline uint16_t get_rows()
	{
		return rows;
	}

	inline uint16_t get_tiles()
	{
		return tiles;
	}

	inline int lines_remaining()
	{
		return rows - (cursor_position / columns) - 1;
	}

	inline int get_current_column()
	{
		return cursor_position % columns;
	}

	inline int get_current_row()
	{
		return cursor_position / columns;
	}
};

}

#endif
