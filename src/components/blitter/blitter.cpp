/*
 * blitter.cpp
 * E64
 *
 * Copyright © 2020-2022 elmerucr. All rights reserved.
 */

#include "blitter.hpp"
#include "rom.hpp"
#include "common.hpp"

E64::blitter_ic::blitter_ic()
{
	fb = new uint16_t[TOTAL_PIXELS];

	general_ram =               new uint8_t [GENERAL_RAM_ELEMENTS];			//   2mb
	tile_ram  =                 new uint8_t [TILE_RAM_ELEMENTS];			//   2mb
	tile_foreground_color_ram = new uint16_t[TILE_FOREGROUND_COLOR_RAM_ELEMENTS];	//   2mb
	tile_background_color_ram = new uint16_t[TILE_BACKGROUND_COLOR_RAM_ELEMENTS];	//   2mb
	pixel_ram =                 new uint16_t[PIXEL_RAM_ELEMENTS];			//   8mb +
											// ============
											//  16mb total

	/*
	 * Fill blit memory alternating 64 bytes 0x00 and 64 bytes 0xff
	 */
	for (int i=0; i < (0x100 * 0x10000); i++) {
		video_memory_write_8(i, i & 0b1000000 ? 0xff : 0x00);
	}

	/*
	 * Array of 256 blits
	 */
	blit = new struct blit_t[256];

	for (int i=0; i<256; i++) {
		blit[i].number          = i;

		blit[i].background      = false;
		blit[i].multicolor_mode = false;
		blit[i].color_per_tile  = false;
		blit[i].use_cbm_font    = false;

		blit[i].flags_1         = 0;
		blit[i].process_flags_1();
//		blit[i].hor_stretch     = false;
//		blit[i].ver_stretch     = false;
//		blit[i].hor_flip        = false;
//		blit[i].ver_flip        = false;
//		blit[i].rotate          = false;

		blit[i].set_size_in_pixels_log2(0);
		blit[i].set_tile_size_in_pixels_log2(0);
		blit[i].calculate_dimensions();

		blit[i].foreground_color = 0;
		blit[i].background_color = 0;
		blit[i].x_pos = 0;
		blit[i].y_pos = 0;
	}

	/*
	 * cbm font as bitmap
	 */
	cbm_font = new uint16_t[256 * 64];

	/*
	 * Convert the character rom to a 16bit argb4444 format
	 */
	uint16_t *dest = cbm_font;
	for (int i=0; i<2048; i++) {
		uint8_t byte = cbm_cp437_font[i];
		uint8_t count = 8;
		while (count--) {
			*dest = (byte & 0b10000000) ? C64_GREY : 0x0000;
			dest++;
			byte = byte << 1;
		}
	}
	
	exceptions_connected = false;
}

E64::blitter_ic::~blitter_ic()
{
	delete [] cbm_font;
	delete [] blit;
	delete [] pixel_ram;
	delete [] tile_background_color_ram;
	delete [] tile_foreground_color_ram;
	delete [] tile_ram;
	delete [] general_ram;
	delete [] fb;
}

void E64::blitter_ic::connect_exceptions_ic(exceptions_ic *exceptions_unit)
{
	exceptions = exceptions_unit;
	irq_number = exceptions->connect_device();
	exceptions_connected = true;
}

void E64::blitter_ic::reset()
{
	fsm_blitter_state = FSM_IDLE;

	head = 0;
	tail = 0;

	for (int i=0; i<TOTAL_PIXELS; i++) {
		fb[i] = 0xf222;
	}

	pending_screenrefresh_irq = false;
	generate_screenrefresh_irq = false;
	
	if (exceptions_connected) exceptions->release(irq_number);
}

inline void E64::blitter_ic::check_new_operation()
{
	if (head != tail) {
		switch (operations[tail].type) {
			case CLEAR:
				fsm_blitter_state = FSM_CLEARING;
				fsm_total_no_of_pix = PIXELS_PER_SCANLINE * SCANLINES;
				break;
			case HOR_BORDER:
				fsm_blitter_state = FSM_DRAW_HOR_BORDER;
				fsm_total_no_of_pix = PIXELS_PER_SCANLINE * hor_border_size;
				break;
			case VER_BORDER:
				fsm_blitter_state = FSM_DRAW_VER_BORDER;
				fsm_total_no_of_pix = SCANLINES * ver_border_size;
				break;
			case BLIT:
				fsm_blitter_state = FSM_BLITTING;
				fsm_current_blit = &operations[tail].blit;
				fsm_total_no_of_pix = fsm_current_blit->width_on_screen * fsm_current_blit->height_on_screen;
				break;
		}
		pixel = 0;
		tail++;
	}
}

void E64::blitter_ic::run(int no_of_cycles)
{
	while (no_of_cycles > 0) {
		no_of_cycles--;

		switch (fsm_blitter_state) {
		case FSM_IDLE:
			check_new_operation();
			break;
		case FSM_CLEARING:
			if (pixel != fsm_total_no_of_pix) {
				fb[pixel] = clear_color;
				pixel++;
			} else {
				fsm_blitter_state = FSM_IDLE;
			}
			break;
		case FSM_DRAW_HOR_BORDER:
			if (pixel != fsm_total_no_of_pix) {
				host.video->alpha_blend(&fb[pixel], &hor_border_color);
				host.video->alpha_blend(&fb[(TOTAL_PIXELS-1) - pixel], &hor_border_color);
				pixel++;
			} else {
				fsm_blitter_state = FSM_IDLE;
			}
			break;
		case FSM_DRAW_VER_BORDER:
			if (pixel != fsm_total_no_of_pix) {
				uint32_t norm_pixel = (pixel % ver_border_size) + ((pixel / ver_border_size) * PIXELS_PER_SCANLINE);
				host.video->alpha_blend(&fb[norm_pixel], &ver_border_color);
				host.video->alpha_blend(&fb[(TOTAL_PIXELS-1) - norm_pixel], &ver_border_color);
				pixel++;
			} else {
				fsm_blitter_state = FSM_IDLE;
			}
			break;
		case FSM_BLITTING:
			if (pixel != fsm_total_no_of_pix) {
				if (fsm_current_blit->hor_flip) {
					fsm_temp_x = fsm_current_blit->width_on_screen - (pixel & fsm_current_blit->width_on_screen_mask) - 1;
				} else {
					fsm_temp_x = pixel & fsm_current_blit->width_on_screen_mask;
				}
				
				if (fsm_current_blit->ver_flip) {
					fsm_temp_y = fsm_current_blit->height_on_screen - (pixel >> fsm_current_blit->width_on_screen_log2) - 1;
				} else {
					fsm_temp_y = pixel >> fsm_current_blit->width_on_screen_log2;
				}
				
				/*
				 * Rotate transformation. Max rotation is 90 degrees
				 * to the right, all others are combinations of this
				 * rotation and flips.
				 */
				if (fsm_current_blit->rotate) {
					fsm_temp_holder = fsm_temp_y;
					fsm_temp_y = fsm_temp_x;
					fsm_temp_x = fsm_current_blit->height_on_screen - fsm_temp_holder;
				}
				
				fsm_scrn_x = fsm_current_blit->x_pos + fsm_temp_x;

				if (fsm_scrn_x < PIXELS_PER_SCANLINE) {     // clipping check horizontally
					fsm_scrn_y = fsm_current_blit->y_pos + fsm_temp_y;

					if (fsm_scrn_y < SCANLINES) {       // clipping check vertically
						/*
						 * Transformation of pixel_no to take into account double width and/or height. After
						 * this <complex> transformation, the normalized_pixel_no points to a position in the
						 * source material.
						 * NEEDS WORK: document this transformation properly
						 */
						fsm_normalized_pixel_no = (((pixel >> (fsm_current_blit->ver_stretch ? 1 : 0)) & ~fsm_current_blit->width_on_screen_mask) | (pixel & fsm_current_blit->width_on_screen_mask)) >> (fsm_current_blit->hor_stretch ? 1 : 0);

						/*
						 * Calculate the current x and y positions within
						 * the current blit source pixeldata.
						 */
						x_in_blit = fsm_normalized_pixel_no & fsm_current_blit->width_mask;
						y_in_blit = fsm_normalized_pixel_no >> (fsm_current_blit->width_log2);

						tile_x = x_in_blit >> (fsm_current_blit->tile_width_log2);
						tile_y = y_in_blit >> (fsm_current_blit->tile_height_log2);

						tile_number = tile_x + (tile_y << (fsm_current_blit->width_in_tiles_log2));

						tile_index = tile_ram[((fsm_current_blit->number << 13) + tile_number) & TILE_RAM_ELEMENTS_MASK];

						/*
						 * Replace foreground and background colors
						 * if color per tile.
						 */
						if (fsm_current_blit->color_per_tile) {
							fsm_current_blit->foreground_color = tile_foreground_color_ram[((fsm_current_blit->number << 12) + tile_number) & TILE_FOREGROUND_COLOR_RAM_ELEMENTS_MASK];
							fsm_current_blit->background_color = tile_background_color_ram[((fsm_current_blit->number << 12) + tile_number) & TILE_BACKGROUND_COLOR_RAM_ELEMENTS_MASK];
						}

						pixel_in_tile = (x_in_blit & (fsm_current_blit->tile_width_mask)) | ((y_in_blit & (fsm_current_blit->tile_height_mask)) << (fsm_current_blit->tile_width_log2));

						/*
						 * Pick the right pixel from blit depending on bitmap mode or tile mode,
						 * and based on cbm_font or not
						 */
						if (fsm_current_blit->use_cbm_font) {
							source_color = cbm_font[((tile_index << 6) | pixel_in_tile) & 0x3fff];
						} else {
							source_color = pixel_ram[((fsm_current_blit->number << 14) + ((tile_index << (fsm_current_blit->tile_width_log2 + fsm_current_blit->tile_height_log2)) | pixel_in_tile)) & PIXEL_RAM_ELEMENTS_MASK];
						}

						/*
						 * Check for multicolor or simple color
						 *
						 * If the source color has an alpha value of higher than 0x0 (pixel present),
						 * and not in multicolor mode, replace with foreground color.
						 *
						 * If there's no alpha value (no pixel), and we have background 'on',
						 * replace the color with background color.
						 *
						 * At this stage, we have already checked for color per
						 * tile, and if so, the value of foreground and respectively
						 * background color have been replaced accordingly.
						 */
						if (source_color & 0xf000) {
							if (!fsm_current_blit->multicolor_mode) source_color = fsm_current_blit->foreground_color;
						} else {
							if (fsm_current_blit->background) source_color = fsm_current_blit->background_color;
						}

						/*
						 * Finally, call the alpha blend function
						 */
						host.video->alpha_blend(&fb[fsm_scrn_x + (fsm_scrn_y * PIXELS_PER_SCANLINE)], &source_color);
					}
				}
				pixel++;
			} else {
				fsm_blitter_state = FSM_IDLE;
			}
			break;
		}
	}
}

void E64::blitter_ic::set_clear_color(uint16_t color)
{
	clear_color = color;
}

void E64::blitter_ic::add_operation_clear_framebuffer()
{
	operations[head].type = CLEAR;
	// leaves blit structure for what it is
	head++;
}

void E64::blitter_ic::add_operation_draw_hor_border()
{
	operations[head].type = HOR_BORDER;
	// leaves blit structure for what it is
	head++;
}

void E64::blitter_ic::add_operation_draw_ver_border()
{
	operations[head].type = VER_BORDER;
	// leaves blit structure for what it is
	head++;
}

void E64::blitter_ic::add_operation_draw_blit(blit_t *blit)
{
	operations[head].type = BLIT;
	operations[head].blit = *blit;
	head++;
}

uint8_t E64::blitter_ic::io_read_8(uint16_t address)
{
	switch (address & 0xe0) {
		case 0x00:
			switch (address & 0x1f) {
				case BLIT_SR:
					if (pending_screenrefresh_irq) {
						return 0b00000001;
					} else {
						return 0b00000000;
					}
				case BLITTER_CR:
					if (generate_screenrefresh_irq) {
						return 0b00000001;
					} else {
						return 0b00000000;
					}
				case BLITTER_HOR_BORDER_SIZE:
					return hor_border_size;
				case BLITTER_VER_BORDER_SIZE:
					return ver_border_size;
				case BLITTER_HOR_BORDER_COLOR:
					return (hor_border_color & 0xff00) >> 8;
				case BLITTER_HOR_BORDER_COLOR+1:
					return hor_border_color & 0xff;
				case BLITTER_VER_BORDER_COLOR:
					return (ver_border_color & 0xff00) >> 8;
				case BLITTER_VER_BORDER_COLOR+1:
					return ver_border_color & 0xff;
				case BLITTER_CLEAR_COLOR:
					return (clear_color & 0xff00) >> 8;
				case BLITTER_CLEAR_COLOR+1:
					return clear_color & 0xff;
				case BLITTER_CONTEXT_0:
					return blitter_context_0;
				case BLITTER_CONTEXT_1:
					return blitter_context_1;
				case BLITTER_CONTEXT_2:
					return blitter_context_2;
				case BLITTER_CONTEXT_3:
					return blitter_context_3;
				case BLITTER_CONTEXT_4:
					return blitter_context_4;
				case BLITTER_CONTEXT_5:
					return blitter_context_5;
				case BLITTER_CONTEXT_6:
					return blitter_context_6;
				default:
					return 0;
			}
		case 0x20:
			return io_blit_context_read_8(blitter_context_0, address & 0x1f);
		case 0x40:
			return io_blit_context_read_8(blitter_context_1, address & 0x1f);
		case 0x60:
			return io_blit_context_read_8(blitter_context_2, address & 0x1f);
		case 0x80:
			return io_blit_context_read_8(blitter_context_3, address & 0x1f);
		case 0xa0:
			return io_blit_context_read_8(blitter_context_4, address & 0x1f);
		case 0xc0:
			return io_blit_context_read_8(blitter_context_5, address & 0x1f);
		case 0xe0:
			return io_blit_context_read_8(blitter_context_6, address & 0x1f);
		default:
			return 0;
	}
}

void E64::blitter_ic::io_write_8(uint16_t address, uint8_t byte)
{
	switch (address & 0xe0) {
		case 0x00:
			switch (address & 0x1f) {
				case BLITTER_SR:
					if ((byte & 0b00000001) && pending_screenrefresh_irq) {
						pending_screenrefresh_irq = false;
						if (exceptions_connected) {
							exceptions->release(irq_number);
						}
					}
					break;
				case BLITTER_CR:
					if (byte & 0b00000001) {
						generate_screenrefresh_irq = true;
					} else {
						generate_screenrefresh_irq = false;
					}
					break;
				case BLITTER_TASK:
					if (byte & 0b00000001) add_operation_clear_framebuffer();
					if (byte & 0b00000010) add_operation_draw_hor_border();
					if (byte & 0b00000100) add_operation_draw_ver_border();
					break;
				case BLITTER_HOR_BORDER_SIZE:
					hor_border_size = byte;
					break;
				case BLITTER_VER_BORDER_SIZE:
					ver_border_size = byte;
					break;
				case BLITTER_HOR_BORDER_COLOR:
					hor_border_color = (hor_border_color & 0x00ff) | (byte << 8);
					break;
				case BLITTER_HOR_BORDER_COLOR+1:
					hor_border_color = (hor_border_color & 0xff00) | byte;
					break;
				case BLITTER_VER_BORDER_COLOR:
					ver_border_color = (ver_border_color & 0x00ff) | (byte << 8);
					break;
				case BLITTER_VER_BORDER_COLOR+1:
					ver_border_color = (ver_border_color & 0xff00) | byte;
					break;
				case BLITTER_CLEAR_COLOR:
					clear_color = (clear_color & 0x00ff) | (byte << 8);
					break;
				case BLITTER_CLEAR_COLOR+1:
					clear_color = (clear_color & 0xff00) | byte;
					break;
				case BLITTER_CONTEXT_0:
					blitter_context_0 = byte;
					break;
				case BLITTER_CONTEXT_1:
					blitter_context_1 = byte;
					break;
				case BLITTER_CONTEXT_2:
					blitter_context_2 = byte;
					break;
				case BLITTER_CONTEXT_3:
					blitter_context_3 = byte;
					break;
				case BLITTER_CONTEXT_4:
					blitter_context_4 = byte;
					break;
				case BLITTER_CONTEXT_5:
					blitter_context_5 = byte;
					break;
				case BLITTER_CONTEXT_6:
					blitter_context_6 = byte;
					break;
			}
			break;
		case 0x20:
			io_blit_context_write_8(blitter_context_0, address & 0x1f, byte);
			break;
		case 0x40:
			io_blit_context_write_8(blitter_context_1, address & 0x1f, byte);
			break;
		case 0x60:
			io_blit_context_write_8(blitter_context_2, address & 0x1f, byte);
			break;
		case 0x80:
			io_blit_context_write_8(blitter_context_3, address & 0x1f, byte);
			break;
		case 0xa0:
			io_blit_context_write_8(blitter_context_4, address & 0x1f, byte);
			break;
		case 0xc0:
			io_blit_context_write_8(blitter_context_5, address & 0x1f, byte);
			break;
		case 0xe0:
			io_blit_context_write_8(blitter_context_6, address & 0x1f, byte);
			break;
		default:
			// do nothing
			break;
	}
}

uint8_t E64::blitter_ic::io_blit_context_read_8(uint8_t blit_no, uint8_t address)
{
	switch (address & 0x1f) {
		case BLIT_SR:
			// reading status register tells something about cursor
			uint8_t temp_byte;
			temp_byte =
				((blit[blit_no].cursor_position == 0) ? 0x80 : 0x00) |
				((blit[blit_no].cursor_position % blit[blit_no].columns == 0) ? 0x40 : 0x00) |
				(blit[blit_no].cursor_big_move ? 0x20 : 0x00);
			return temp_byte;
		case BLIT_CR:
			// maybe not used
			return 0;
		case BLIT_FLAGS_0:
			return	(blit[blit_no].background      ? 0x02 : 0x00) |
				(blit[blit_no].multicolor_mode ? 0x04 : 0x00) |
				(blit[blit_no].color_per_tile  ? 0x08 : 0x00) |
				(blit[blit_no].use_cbm_font    ? 0x80 : 0x00) ;
		case BLIT_FLAGS_1:
			return blit[blit_no].flags_1;
//			return  (blit[blit_no].hor_stretch   ? 0x01 : 0x00) |
//				(blit[blit_no].ver_stretch   ? 0x02 : 0x00) |
//				(blit[blit_no].hor_flip      ? 0x10 : 0x00) |
//				(blit[blit_no].ver_flip      ? 0x20 : 0x00) |
//				(blit[blit_no].rotate        ? 0x40 : 0x00) ;
		case BLIT_SIZE_PIXELS_LOG2:
			return blit[blit_no].get_size_in_pixels_log2();
		case BLIT_TILE_SIZE_LOG2:
			return blit[blit_no].get_tile_size_in_pixels_log2();
		case BLIT_FG_COLOR_MSB:
			return ((blit[blit_no].foreground_color) & 0xff00) >> 8;
		case BLIT_FG_COLOR_LSB:
			return (blit[blit_no].foreground_color) & 0x00ff;
		case BLIT_BG_COLOR_MSB:
			return ((blit[blit_no].background_color) & 0xff00) >> 8;
		case BLIT_BG_COLOR_LSB:
			return (blit[blit_no].background_color) & 0x00ff;
		case BLIT_XPOS_MSB:
			return (((uint16_t)blit[blit_no].x_pos) & 0xff00) >> 8;
		case BLIT_XPOS_LSB:
			return (((uint16_t)blit[blit_no].x_pos) & 0x00ff);
		case BLIT_YPOS_MSB:
			return (((uint16_t)blit[blit_no].y_pos) & 0xff00) >> 8;
		case BLIT_YPOS_LSB:
			return (((uint16_t)blit[blit_no].y_pos) & 0x00ff);
		case BLIT_NO_OF_TILES_MSB:
			return (blit[blit_no].tiles & 0xff00) >> 8;
		case BLIT_NO_OF_TILES_LSB:
			return blit[blit_no].tiles & 0xff;
		case BLIT_CURSOR_POS_MSB:
			// cursor position of current blit (reg 0x01), high byte
			return (blit[blit_no].cursor_position & 0xff00) >> 8;
		case BLIT_CURSOR_POS_LSB:
			// cursor position of current blit (reg 0x01), low byte
			return blit[blit_no].cursor_position & 0xff;
		case BLIT_CURSOR_BLINK_SPEED:
			// return cursor blink interval;
			return blit[blit_no].cursor_interval;
		case BLIT_PITCH:
			// return the pitch of current blit (reg 0x01), max 128, fits in one byte
			return blit[blit_no].columns;
		case BLIT_CURSOR_CHAR:
			// character at cursor pos
			return tile_ram[((blit_no << 13) + blit[blit_no].cursor_position) & TILE_RAM_ELEMENTS_MASK];
		case BLIT_CURSOR_FG_COLOR_MSB:
			// foreground color at cursor msb
			return (tile_foreground_color_ram[((blit_no << 12) + blit[blit_no].cursor_position) & TILE_FOREGROUND_COLOR_RAM_ELEMENTS_MASK] & 0xff00) >> 8;
		case BLIT_CURSOR_FG_COLOR_LSB:
			// foreground color at cursor lsb
			return tile_foreground_color_ram[((blit_no << 12) + blit[blit_no].cursor_position) & TILE_FOREGROUND_COLOR_RAM_ELEMENTS_MASK] & 0x00ff;
		case BLIT_CURSOR_BG_COLOR_MSB:
			// background color at cursor msb
			return (tile_background_color_ram[((blit_no << 12) + blit[blit_no].cursor_position) & TILE_FOREGROUND_COLOR_RAM_ELEMENTS_MASK] & 0xff00) >> 8;
		case BLIT_CURSOR_BG_COLOR_LSB:
			// background color at cursor lsb
			return tile_background_color_ram[((blit_no << 12) + blit[blit_no].cursor_position) & TILE_FOREGROUND_COLOR_RAM_ELEMENTS_MASK] & 0x00ff;
		default:
			return 0;
	}
}

void E64::blitter_ic::io_blit_context_write_8(uint8_t blit_no, uint8_t address, uint8_t byte)
{
	uint16_t temp_word;

	switch (address & 0x1f) {
		case BLIT_SR:
			// unused so far...
			break;
		case BLIT_CR:
			switch (byte) {
				case 0b00000001:
					add_operation_draw_blit(&blit[blit_no]);
					break;
				case 0b10000000:
					// decrease cursor position
					terminal_cursor_decrease(blit_no);
					break;
				case 0b10000001:
					// increase cursor position
					terminal_cursor_increase(blit_no);
					break;
				case 0b11000000:
					// activate cursor
					terminal_activate_cursor(blit_no);
					break;
				case 0b11000001:
					// deactivate cursor
					terminal_deactivate_cursor(blit_no);
					break;
				case 0b11000010:
					// process cursor state
					terminal_process_cursor_state(blit_no);
					break;
				default:
					break;
			}
			break;
		case BLIT_FLAGS_0:
			/*
			 * flags 0
			 */
			blit[blit_no].background      = byte & 0x02 ? true : false;
			blit[blit_no].multicolor_mode = byte & 0x04 ? true : false;
			blit[blit_no].color_per_tile  = byte & 0x08 ? true : false;
			blit[blit_no].use_cbm_font    = byte & 0x80 ? true : false;
			break;
		case BLIT_FLAGS_1:
			/*
			 * flags 1
			 */
			blit[blit_no].flags_1 = byte;
			blit[blit_no].process_flags_1();
//			blit[blit_no].hor_stretch   = byte & 0x01  ? true : false;
//			blit[blit_no].ver_stretch   = byte & 0x02  ? true : false;
//			blit[blit_no].hor_flip      = byte & 0x10  ? true : false;
//			blit[blit_no].ver_flip      = byte & 0x20  ? true : false;
//			blit[blit_no].rotate        = byte & 0x40  ? true : false;
			blit[blit_no].calculate_dimensions();
			break;
		case BLIT_SIZE_PIXELS_LOG2:
			blit[blit_no].set_size_in_pixels_log2(byte);
			break;
		case BLIT_TILE_SIZE_LOG2:
			blit[blit_no].set_tile_size_in_pixels_log2(byte);
			break;
		case BLIT_FG_COLOR_MSB:
			temp_word = (blit[blit_no].foreground_color) & 0x00ff;
			blit[blit_no].foreground_color = temp_word | (byte << 8);
			break;
		case BLIT_FG_COLOR_LSB:
			temp_word = (blit[blit_no].foreground_color) & 0xff00;
			blit[blit_no].foreground_color = temp_word | byte;
			break;
		case BLIT_BG_COLOR_MSB:
			temp_word = (blit[blit_no].background_color) & 0x00ff;
			blit[blit_no].background_color = temp_word | (byte << 8);
			break;
		case BLIT_BG_COLOR_LSB:
			temp_word = (blit[blit_no].background_color) & 0xff00;
			blit[blit_no].background_color = temp_word | byte;
			break;
		case BLIT_XPOS_MSB:
			temp_word = (blit[blit_no].x_pos) & 0x00ff;
			blit[blit_no].x_pos = temp_word | (byte << 8);
			break;
		case BLIT_XPOS_LSB:
			temp_word = (blit[blit_no].x_pos) & 0xff00;
			blit[blit_no].x_pos = temp_word | byte;
			break;
		case BLIT_YPOS_MSB:
			temp_word = (blit[blit_no].y_pos) & 0x00ff;
			blit[blit_no].y_pos = temp_word | (byte << 8);
			break;
		case BLIT_YPOS_LSB:
			temp_word = (blit[blit_no].y_pos) & 0xff00;
			blit[blit_no].y_pos = temp_word | byte;
			break;
		case BLIT_CURSOR_POS_MSB:
			blit[blit_no].cursor_position =
			(blit[blit_no].cursor_position & 0x00ff) | (byte << 8);
			break;
		case BLIT_CURSOR_POS_LSB:
			blit[blit_no].cursor_position =
			(blit[blit_no].cursor_position & 0xff00) | byte;
			break;
		case BLIT_CURSOR_BLINK_SPEED:
			// set cursor blink interval
			blit[blit_no].cursor_interval = byte;
			break;
		case BLIT_CURSOR_CHAR:
			// character at cursor pos
			tile_ram[((blit_no << 13) + blit[blit_no].cursor_position) & TILE_RAM_ELEMENTS_MASK] = byte;
			break;
		case BLIT_CURSOR_FG_COLOR_MSB:
			// foreground color at cursor msb
			tile_foreground_color_ram[((blit_no << 12) + blit[blit_no].cursor_position) & TILE_FOREGROUND_COLOR_RAM_ELEMENTS_MASK] =
			(tile_foreground_color_ram[((blit_no << 12) + blit[blit_no].cursor_position) & TILE_FOREGROUND_COLOR_RAM_ELEMENTS_MASK] & 0x00ff) | (byte << 8);
			break;
		case BLIT_CURSOR_FG_COLOR_LSB:
			// foreground color at cursor lsb
			tile_foreground_color_ram[((blit_no << 12) + blit[blit_no].cursor_position) & TILE_FOREGROUND_COLOR_RAM_ELEMENTS_MASK] =
			(tile_foreground_color_ram[((blit_no << 12) + blit[blit_no].cursor_position) & TILE_FOREGROUND_COLOR_RAM_ELEMENTS_MASK] & 0xff00) | byte;
			break;
		case BLIT_CURSOR_BG_COLOR_MSB:
			// background color at cursor msb
			tile_background_color_ram[((blit_no << 12) + blit[blit_no].cursor_position) & TILE_FOREGROUND_COLOR_RAM_ELEMENTS_MASK] =
			(tile_background_color_ram[((blit_no << 12) + blit[blit_no].cursor_position) & TILE_FOREGROUND_COLOR_RAM_ELEMENTS_MASK] & 0x00ff) | (byte << 8);
			break;
		case BLIT_CURSOR_BG_COLOR_LSB:
			// background color at cursor lsb
			tile_background_color_ram[((blit_no << 12) + blit[blit_no].cursor_position) & TILE_FOREGROUND_COLOR_RAM_ELEMENTS_MASK] =
			(tile_background_color_ram[((blit_no << 12) + blit[blit_no].cursor_position) & TILE_FOREGROUND_COLOR_RAM_ELEMENTS_MASK] & 0xff00) | byte;
			break;
		default:
			// do nothing
			break;
	}
}

void E64::blitter_ic::notify_screen_refreshed()
{
	// do something with interrupt line (if enabled)
	if (generate_screenrefresh_irq && exceptions_connected) {
		pending_screenrefresh_irq = true;
		exceptions->pull(irq_number);
	}
}
