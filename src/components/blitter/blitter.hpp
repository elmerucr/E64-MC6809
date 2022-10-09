/*
 * blitter.hpp
 * E64
 *
 * Copyright © 2020-2022 elmerucr. All rights reserved.
 */

/*
 * Blitter is able to copy data very fast from video memory location to
 * backbuffer (framebuffer). Copy operations run independently and can
 * be added to a FIFO linked list (blitter internally).
 */

/*
 * I/O addresses blitter_ic
 *
 * ==============================================================================
 * General
 * ============================================================================== */
   #define BLITTER_SR			0x00	// blitter status register (pending irq's)
   #define BLITTER_CR			0x01	// blitter control register (irq activation)
   #define BLITTER_TASK			0x02	// initiate clear buffer or border draws
   #define BLITTER_HOR_BORDER_SIZE	0x08	// horizontal border size
   #define BLITTER_VER_BORDER_SIZE	0x09	// vertical border size
   #define BLITTER_HOR_BORDER_COLOR	0x0a	// horizontal border color
   #define BLITTER_VER_BORDER_COLOR	0x0c	// vertical border color
   #define BLITTER_CLEAR_COLOR		0x0e	// clear color (background color)
   #define BLITTER_CONTEXT_0		0x10	// seen from 0x20-0x3f (also the active one for CR)
   #define BLITTER_CONTEXT_1		0x11	// seen from 0x40-0x5f
   #define BLITTER_CONTEXT_2		0x12	// seen from 0x60-0x7f
   #define BLITTER_CONTEXT_3		0x13	// seen from 0x80-0x9f
   #define BLITTER_CONTEXT_4		0x14	// seen from 0xa0-0xbf
   #define BLITTER_CONTEXT_5		0x15	// seen from 0xc0-0xdf
   #define BLITTER_CONTEXT_6		0x16	// seen from 0xe0-0xff

/* =========================================================================================
 * Individual blit context registers (based on context registers), relative to start context
 * ========================================================================================= */
   #define BLIT_SR			0x00	// SR probably not used here
   #define BLIT_CR			0x01	// control register
   #define BLIT_FLAGS_0			0x02	// flags 0
   #define BLIT_FLAGS_1			0x03	// flags 1
   #define BLIT_SIZE_PIXELS_LOG2	0x04	// size_in_pixels_log2
   #define BLIT_TILE_SIZE_LOG2		0x05	// tile_size_log2
   #define BLIT_FG_COLOR_MSB		0x08	// current foreground color hb
   #define BLIT_FG_COLOR_LSB		0x09	// current foreground color lb
   #define BLIT_BG_COLOR_MSB		0x0a	// current background color hb
   #define BLIT_BG_COLOR_LSB		0x0b	// current background color hb
   #define BLIT_XPOS_MSB		0x0c	// xpos hb
   #define BLIT_XPOS_LSB		0x0d	// xpos lb
   #define BLIT_YPOS_MSB		0x0e	// ypos hb
   #define BLIT_YPOS_LSB		0x0f	// ypos lb
   #define BLIT_NO_OF_TILES_MSB		0x10
   #define BLIT_NO_OF_TILES_LSB		0x11
   #define BLIT_CURSOR_POS_MSB		0x12
   #define BLIT_CURSOR_POS_LSB		0x13
   #define BLIT_CURSOR_BLINK_SPEED	0x14
   #define BLIT_PITCH			0x15	// pitch = width of blit in characters
   #define BLIT_CURSOR_CHAR		0x16	// tile at cursor, read/write
   #define BLIT_CURSOR_FG_COLOR_MSB	0x18
   #define BLIT_CURSOR_FG_COLOR_LSB	0x19
   #define BLIT_CURSOR_BG_COLOR_MSB	0x1a
   #define BLIT_CURSOR_BG_COLOR_LSB	0x1b

/*
 * 0x20, 0x40, 0x60, 0x80, 0xa0, 0xc0, 0xe0: start of blit contexts
 */

#ifndef BLITTER_HPP
#define BLITTER_HPP

#define	GENERAL_RAM_ELEMENTS			0x200000	// each 1 byte,  2mb, starts @ $000000 to $1fffff
#define TILE_RAM_ELEMENTS			0x200000	// each 1 byte,  2mb, starts @ $200000 to $3fffff, steps of $2000
#define TILE_FOREGROUND_COLOR_RAM_ELEMENTS	0x100000	// each 2 bytes, 2mb, starts @ $400000 to $5fffff, steps of $2000
#define TILE_BACKGROUND_COLOR_RAM_ELEMENTS	0x100000	// each 2 bytes, 2mb, starts @ $600000 to $7fffff, steps of $2000
#define PIXEL_RAM_ELEMENTS			0x400000	// each 2 bytes, 8mb, starts @ $800000 to $ffffff, steps of $8000

#define	GENERAL_RAM_ELEMENTS_MASK		(GENERAL_RAM_ELEMENTS-1)
#define TILE_RAM_ELEMENTS_MASK			(TILE_RAM_ELEMENTS-1)
#define TILE_FOREGROUND_COLOR_RAM_ELEMENTS_MASK	(TILE_FOREGROUND_COLOR_RAM_ELEMENTS-1)
#define TILE_BACKGROUND_COLOR_RAM_ELEMENTS_MASK	(TILE_BACKGROUND_COLOR_RAM_ELEMENTS-1)
#define PIXEL_RAM_ELEMENTS_MASK			(PIXEL_RAM_ELEMENTS-1)

#include "blit.hpp"
#include "exceptions.hpp"

namespace E64
{

enum operation_type {
	CLEAR,
	HOR_BORDER,
	VER_BORDER,
	BLIT
};

struct operation {
	enum operation_type type;
	blit_t blit;
};

enum fsm_blitter_state_t {
	FSM_IDLE,
	FSM_CLEARING,
	FSM_DRAW_HOR_BORDER,
	FSM_DRAW_VER_BORDER,
	FSM_BLITTING
};

class blitter_ic {
private:
	bool exceptions_connected;
	//uint8_t irq_number;
	bool pending_screenrefresh_irq;
	bool generate_screenrefresh_irq;
	exceptions_ic *exceptions;

	// video ram, different components
	uint8_t  *general_ram;			// 2mb
	uint8_t  *tile_ram;			// 2mb
	uint16_t *tile_foreground_color_ram;	// 2mb
	uint16_t *tile_background_color_ram;	// 2mb
	uint16_t *pixel_ram;			// 8mb

	uint16_t *cbm_font;	// pointer to unpacked font

	/*
	 * Specific for border
	 */
	uint8_t  hor_border_size;
	uint8_t  ver_border_size;

	uint16_t hor_border_color;
	uint16_t ver_border_color;

	/*
	 * Specific for clearing framebuffer / background color
	 */
	uint16_t clear_color;

	/*
	 * Blitter contexts visible from 0x20-0xff
	 */
	uint8_t  blitter_context_0;
	uint8_t  blitter_context_1;
	uint8_t  blitter_context_2;
	uint8_t  blitter_context_3;
	uint8_t  blitter_context_4;
	uint8_t  blitter_context_5;
	uint8_t  blitter_context_6;

	enum fsm_blitter_state_t fsm_blitter_state;

	inline void check_new_operation();

	/*
	 * Circular buffer containing max 65536 operations. If more
	 * operations would be written (unlikely) and unfinished, buffer
	 * will overwrite itself.
	 */
	struct operation operations[65536];
	uint16_t head;
	uint16_t tail;

	/*
	 * Finite state machine
	 */
	blit_t   *fsm_current_blit;

	uint32_t fsm_total_no_of_pix;		// total number of pixels to blit onto framebuffer for current blit
	uint32_t pixel;				// current pixel of the total that is being processed
	uint32_t fsm_normalized_pixel_no;	// normalized to the dimensions of source

	/*
	 *
	 */
	uint16_t fsm_temp_x;
	uint16_t fsm_temp_y;
	uint16_t fsm_temp_holder;
	
	uint16_t fsm_scrn_x;            // final screen x for the current pixel
	uint16_t fsm_scrn_y;            // final screen y for the current pixel

	uint16_t x_in_blit;
	uint16_t y_in_blit;

	uint16_t tile_x;
	uint16_t tile_y;

	uint16_t tile_number;
	uint8_t  tile_index;
	uint16_t current_background_color;
	uint32_t pixel_in_tile;

	uint16_t source_color;
public:
	blitter_ic();
	~blitter_ic();
	
	uint8_t irq_number;
	
	void connect_exceptions_ic(exceptions_ic *exceptions_unit);

	// framebuffer pointer
	uint16_t *fb;

	/*
	 * This method is called to notify blitter that screen was
	 * just refreshed, so it has the possibility to raise an
	 * interrupt (if this is enabled).
	 */
	void notify_screen_refreshed();

	/*
	 * io access to blitter_ic (mapped to a specific page in
	 * cpu memory)
	 */
	uint8_t	io_read_8 (uint16_t address);
	void    io_write_8(uint16_t address, uint8_t byte);

	uint8_t io_blit_context_read_8 (uint8_t blit, uint8_t address);
	void    io_blit_context_write_8(uint8_t blit, uint8_t address, uint8_t byte);

	/*
	 * io access to blit contexts (8k)
	 */
	inline uint8_t io_blit_contexts_read_8(uint16_t address)
	{
		return io_blit_context_read_8((address & 0x1fe0) >> 5, address & 0x1f);
	}

	inline void io_blit_contexts_write_8(uint16_t address, uint8_t byte)
	{
		io_blit_context_write_8((address & 0x1fe0) >> 5, address & 0x1f, byte);
	}

	inline uint8_t video_memory_read_8(uint32_t address)
	{
		switch ((address & 0x00e00000) >> 21) {
			case 0b000:
				/*
				 * General ram
				 */
				return general_ram[address & GENERAL_RAM_ELEMENTS_MASK];
			case 0b001:
				return tile_ram[address & TILE_RAM_ELEMENTS_MASK];
			case 0b010:
				if (address & 0b1) {
					return tile_foreground_color_ram[(address & 0x1fffff) >> 1] & 0x00ff;
				} else {
					return (tile_foreground_color_ram[(address & 0x1fffff) >> 1] & 0xff00) >> 8;
				}
			case 0b011:
				if (address & 0b1) {
					return tile_background_color_ram[(address & 0x1fffff) >> 1] & 0x00ff;
				} else {
					return (tile_background_color_ram[(address & 0x1fffff) >> 1] & 0xff00) >> 8;
				}
			case 0b100:
			case 0b101:
			case 0b110:
			case 0b111:
				/*
				 * Pixel ram
				 */
				if (address & 0b1) {
					return pixel_ram[(address & 0x7fffff) >> 1] & 0x00ff;
				} else {
					return (pixel_ram[(address & 0x7fffff) >> 1] & 0xff00) >> 8;
				}
		}
		return 0;
	}

	inline void video_memory_write_8(uint32_t address, uint8_t value)
	{
		switch ((address & 0x00e00000) >> 21) {
			case 0b000:
				general_ram[address & 0x1fffff] = value;
				break;
			case 0b001:
				tile_ram[address & 0x1fffff] = value;
				break;
			case 0b010:
				if (address & 0b1) {
					tile_foreground_color_ram[(address & 0x1fffff) >> 1] = (tile_foreground_color_ram[(address & 0x1fffff) >> 1] & 0xff00) | value;
				} else {
					tile_foreground_color_ram[(address & 0x1fffff) >> 1] = (tile_foreground_color_ram[(address & 0x1fffff) >> 1] & 0x00ff) | (value << 8);
				}
				break;
			case 0b011:
				if (address & 0b1) {
					tile_background_color_ram[(address & 0x1fffff) >> 1] = (tile_background_color_ram[(address & 0x1fffff) >> 1] & 0xff00) | value;
				} else {
					tile_background_color_ram[(address & 0x1fffff) >> 1] = (tile_background_color_ram[(address & 0x1fffff) >> 1] & 0x00ff) | (value << 8);
				}
				break;
			case 0b100:
			case 0b101:
			case 0b110:
			case 0b111:
				if (address & 0b1) {
					pixel_ram[(address & 0x7fffff) >> 1] = (pixel_ram[(address & 0x7fffff) >> 1] & 0xff00) | value;
				} else {
					pixel_ram[(address & 0x7fffff) >> 1] = (pixel_ram[(address & 0x7fffff) >> 1] & 0x00ff) | (value << 8);
				}
				break;

		}
	}

	void reset();

	void run(int no_of_cycles);

	struct blit_t *blit;

	void set_clear_color(uint16_t color);
	void set_hor_border_color(uint16_t color) { hor_border_color = color; }
	void set_hor_border_size(uint8_t size ) { hor_border_size = size; }

	void add_operation_clear_framebuffer();
	void add_operation_draw_hor_border();
	void add_operation_draw_ver_border();
	void add_operation_draw_blit(blit_t *blit);

	inline bool busy() { return fsm_blitter_state == FSM_IDLE ? false : true; }

	/*
	 * Run cycles until done / not busy anymore.
	 * NEEDS WORK: At low number (100), starts flickering...?
	 */
	inline void flush()
	{
		do run(1000); while (busy());
	}

	void set_pixel(uint8_t number, uint32_t pixel_no, uint16_t color);
	uint16_t get_pixel(uint8_t number, uint32_t pixel_no);

	/*
	 * Terminal interface - Init needs work, flags 0 and flags 1 are sort
	 * of artificial ways to set properties.
	 */
	void     terminal_set_tile(uint8_t number, uint16_t cursor_position, char symbol);
	void     terminal_set_tile_fg_color(uint8_t number, uint16_t cursor_position, uint16_t color);
	void     terminal_set_tile_bg_color(uint8_t number, uint16_t cursor_position, uint16_t color);
	uint8_t  terminal_get_tile(uint8_t number, uint16_t cursor_position);
	uint16_t terminal_get_tile_fg_color(uint8_t number, uint16_t cursor_position);
	uint16_t terminal_get_tile_bg_color(uint8_t number, uint16_t cursor_position);

	void terminal_init(uint8_t number, uint8_t flags_0, uint8_t flags_1,
			   uint8_t size_in_tiles_log2, uint8_t tile_size_log2, uint16_t foreground_color,
			   uint16_t background_color);
	void terminal_clear(uint8_t number);
	void terminal_putsymbol_at_cursor(uint8_t number, char symbol);
	void terminal_putsymbol(uint8_t number, char symbol);
	int  terminal_putchar(uint8_t number, int character);
	int  terminal_puts(uint8_t number, const char *text);
	int  terminal_printf(uint8_t no, const char *format, ...);
	void terminal_prompt(uint8_t number);
	void terminal_activate_cursor(uint8_t number);
	void terminal_deactivate_cursor(uint8_t no);
	void terminal_cursor_decrease(uint8_t no);		// moves to left, wraps around
	void terminal_cursor_increase(uint8_t no);		// moves to the right, and wraps around
	void terminal_cursor_left(uint8_t no);
	void terminal_cursor_right(uint8_t no);
	void terminal_cursor_up(uint8_t no);
	void terminal_cursor_down(uint8_t no);
	void terminal_backspace(uint8_t no);
	void terminal_add_top_row(uint8_t no);
	void terminal_add_bottom_row(uint8_t no);

	void terminal_process_cursor_state(uint8_t no);
	char *terminal_enter_command(uint8_t no);
	enum E64::terminal_output_type terminal_check_output(uint8_t no, bool top_down, uint32_t *address);
};

}

#endif
