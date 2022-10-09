#include "hud.hpp"
#include "common.hpp"
#include "sdl2.hpp"

char text_buffer[2048];

#define	RAM_SIZE_CPU_VISIBLE	0x010000

/*
 * hex2int
 * take a hex string and convert it to a 32bit number (max 8 hex digits)
 * from https://stackoverflow.com/questions/10156409/convert-hex-string-char-to-int
 *
 * This function is slightly adopted to check for true values. It returns false
 * when there's wrong input.
 */
bool E64::hud_t::hex_string_to_int(const char *temp_string, uint32_t *return_value)
{
	uint32_t val = 0;
	while (*temp_string) {
		/* Get current character then increment */
		uint8_t byte = *temp_string++;
		/* Transform hex character to the 4bit equivalent number */
		if (byte >= '0' && byte <= '9') {
			byte = byte - '0';
		} else if (byte >= 'a' && byte <='f') {
			byte = byte - 'a' + 10;
		} else if (byte >= 'A' && byte <='F') {
			byte = byte - 'A' + 10;
		} else {
			/* Problem, return false and do not write the return value */
			return false;
		}
		/* Shift 4 to make space for new digit, and add the 4 bits of the new digit */
		val = (val << 4) | (byte & 0xf);
	}
	*return_value = val;
	return true;
}

E64::hud_t::hud_t()
{
	exceptions = new exceptions_ic();
	blitter = new blitter_ic();
	cia = new cia_ic();
	timer = new timer_ic(exceptions);
	
	stats_view = &blitter->blit[0];
	blitter->terminal_init(stats_view->number, 0x8a, 0x00, 0x58, 0x33, GREEN_05,
				  (GREEN_01 & 0x0fff) | 0xc000);
	
	terminal = &blitter->blit[1];
	blitter->terminal_init(terminal->number, 0x8a, 0x00, 0x79, 0x33, GREEN_05,
				(GREEN_01 & 0x0fff) | 0xc000);
	
	cpu_view = &blitter->blit[2];
	blitter->terminal_init(cpu_view->number, 0x8a, 0x00, 0x49, 0x33, GREEN_05,
				(GREEN_01 & 0x0fff) | 0xc000);
	
	disassembly_view = &blitter->blit[3];
	blitter->terminal_init(disassembly_view->number, 0x8a, 0x00, 0x69, 0x33, GREEN_05,
					(GREEN_01 & 0x0fff) | 0xc000);

	stack_view = &blitter->blit[4];
	blitter->terminal_init(stack_view->number, 0x8a, 0x00, 0x68, 0x33, GREEN_05,
					(GREEN_01 & 0x0fff) | 0x0000);

	bar_64x1 = &blitter->blit[5];
	blitter->terminal_init(bar_64x1->number, 0x02, 0x00, 0x39, 0x39, GREEN_05,
					(GREEN_01 & 0x0fff) | 0xc000);

	bar_64x2 = &blitter->blit[6];
	blitter->terminal_init(bar_64x2->number, 0x8a, 0x00, 0x49, 0x33, GREEN_05,
					(GREEN_01 & 0x0fff) | 0xc000);

	bar_single_height_small = &blitter->blit[7];
	blitter->terminal_init(bar_single_height_small->number, 0x8a, 0x00, 0x38, 0x33, GREEN_05,
					(GREEN_01 & 0x0fff) | 0xc000);
	
	other_info = &blitter->blit[8];
	blitter->terminal_init(other_info->number, 0x8a, 0x00, 0x58, 0x33, GREEN_05,
				  (GREEN_01 & 0x0fff) | 0xc000);
	
	notification = &blitter->blit[9];
	blitter->terminal_init(notification->number, 0x8a, 0x00, 0x59, 0x33, GREEN_05,
				    (GREY_04 & 0x0fff) | 0xd000);
	
	recording_icon = &blitter->blit[10];
	blitter->terminal_init(recording_icon->number, 0x8a, 0x00, 0x36, 0x33, 0xff00, 0x0000);
	
	stats_visible = false;
	stats_pos = 288;
	
	irq_line = true;
	
	notify_frame_counter = 0;
	notify_frames = FPS * 4;	// 4 seconds
	
	frame_is_done = false;
	frame_cycle_saldo = 0;
}

E64::hud_t::~hud_t()
{
	delete timer;
	delete cia;
	delete blitter;
	delete exceptions;
}

void E64::hud_t::reset()
{
	blitter->reset();
	blitter->set_clear_color(0x0000);
	blitter->set_hor_border_color(0x0000);
	blitter->set_hor_border_size(0);
	
	cia->reset();
	cia->set_keyboard_repeat_delay(50);
	cia->set_keyboard_repeat_speed(5);
	cia->generate_key_events();
	
	timer->reset();
	timer->set(0, 3600);	// check keyboard, generate key events...
	
	blitter->terminal_clear(terminal->number);
	blitter->terminal_printf(terminal->number, "E64 Computer System (C)%u elmerucr", E64_YEAR);
	blitter->terminal_prompt(terminal->number);
	blitter->terminal_activate_cursor(terminal->number);
	
	/*
	 * bar_64x1 is one big tile, so make sure first tile = "sprite 0"
	 */
	blitter->terminal_set_tile(bar_64x1->number, 0, 0);
	for (int i=0; i<4096; i++)
		blitter->set_pixel(bar_64x1->number, i, 0x0000);
	for (int i = 1536; i<2048; i++)
		blitter->set_pixel(bar_64x1->number, i, GREEN_05);
	
	blitter->terminal_clear(bar_64x2->number);
	blitter->terminal_clear(bar_single_height_small->number);
	
	blitter->terminal_clear(other_info->number);

	blitter->terminal_clear(recording_icon->number);
	blitter->terminal_printf(recording_icon->number, "rec ");
	blitter->terminal_putsymbol(recording_icon->number, 0x07);
}

void E64::hud_t::process_keypress()
{
	while (cia->io_read_byte(0x00)) {
		blitter->terminal_deactivate_cursor(terminal->number);
	
		uint8_t key_value = cia->io_read_byte(0x04);
		switch (key_value) {
			case ASCII_CURSOR_LEFT:
				blitter->terminal_cursor_left(terminal->number);
				break;
			case ASCII_CURSOR_RIGHT:
				blitter->terminal_cursor_right(terminal->number);
				break;
			case ASCII_CURSOR_UP:
				blitter->terminal_cursor_up(terminal->number);
				break;
			case ASCII_CURSOR_DOWN:
				blitter->terminal_cursor_down(terminal->number);
				break;
			case ASCII_BACKSPACE:
				blitter->terminal_backspace(terminal->number);
				break;
			case ASCII_F1:
				// advance one instruction
				if (machine.mode == E64::PAUSED) {
					machine.run(0);
				}
				break;
			case ASCII_F2:
				// advance eight instructions
				if (machine.mode == E64::PAUSED) {
					for (int i=0; i<8; i++) machine.run(0);
				}
				break;
			case ASCII_F3:
				// advance sixtyfour instructions
				if (machine.mode == E64::PAUSED) {
					for (int i=0; i<64; i++) machine.run(0);
				}
				break;
			case ASCII_LF:
			{
				char *buffer = blitter->terminal_enter_command(terminal->number);
				process_command(buffer);
			}
				break;
			default:
				blitter->terminal_putchar(terminal->number, key_value);
				break;
		}
		blitter->terminal_activate_cursor(terminal->number);
	}
}

void E64::hud_t::update_stats_view()
{
	blitter->terminal_clear(stats_view->number);
	blitter->terminal_puts(stats_view->number, stats.summary());
	if (!machine.buffer_within_specs()) {
		buffer_warning_frame_counter = 20;
	}
	
	if (buffer_warning_frame_counter) {
		for (int i=0x60; i<0x80; i++) {
			blitter->terminal_set_tile_fg_color(stats_view->number, i, 0xff00);
		}
		buffer_warning_frame_counter--;
	}
}

void E64::hud_t::update()
{
	blitter->terminal_clear(cpu_view->number);
	machine.cpu->status(text_buffer);
	blitter->terminal_printf(cpu_view->number, "%s", text_buffer);
	
	blitter->terminal_clear(disassembly_view->number);
	uint16_t pc = machine.cpu->get_pc();
	for (int i=0; i<8; i++) {
		uint16_t old_color = terminal->foreground_color;
		if (machine.cpu->breakpoint_array[pc] == true) disassembly_view->foreground_color = AMBER_07;
		if (disassembly_view->get_current_column() != 0)
			blitter->terminal_putchar(disassembly_view->number, '\n');
		pc += machine.cpu->disassemble_instruction(text_buffer, pc);
		blitter->terminal_printf(disassembly_view->number, "%s", text_buffer);
		//disassembly_view->printf("%s", text_buffer);
		disassembly_view->foreground_color = old_color;
	}
	
	blitter->terminal_clear(stack_view->number);
	//stack_view->terminal_clear();
	machine.cpu->stacks(text_buffer, 7);
	blitter->terminal_printf(stack_view->number, "%s", text_buffer);
	//stack_view->printf("%s", text_buffer);
	
	blitter->terminal_clear(other_info->number);
	//other_info->terminal_clear();
	blitter->terminal_printf(other_info->number, " IRQ lines: blitter(%c) timer(%c)",
			   machine.exceptions->irq_input_pins[machine.blitter->irq_number] ? '1' : '0',
			   machine.exceptions->irq_input_pins[machine.timer->irq_number] ? '1' : '0');
	blitter->terminal_printf(other_info->number, "\n\n cycles done: %u of %u", machine.frame_cycles(), CPU_CYCLES_PER_FRAME);
}

void E64::hud_t::run(uint16_t cycles)
{
	timer->run(cycles);
	if (exceptions->irq_output_pin == false) {
		for (int i=0; i<8; i++) {
			if (timer->io_read_byte(0x00) & (0b1 << i)) {
				switch (i) {
					case 0:
						timer_0_event();
						/*
						 * Acknowledge interrupt
						 */
						timer->io_write_byte(0x00, 0b00000001);
						break;
					case 1:
						timer_1_event();
						/*
						 * Acknowledge interrupt
						 */
						timer->io_write_byte(0x00, 0b00000010);
					default:
						break;
				}
			}
		}
	}
	
	cia->run(cycles);
	
	frame_cycle_saldo += cycles;
	
	if (frame_cycle_saldo > CPU_CYCLES_PER_FRAME) {
		frame_is_done = true;
		frame_cycle_saldo -= CPU_CYCLES_PER_FRAME;
	}
}

void E64::hud_t::timer_0_event()
{
	blitter->terminal_process_cursor_state(terminal->number);
	//terminal->process_cursor_state();
}

void E64::hud_t::timer_1_event()
{

}

void E64::hud_t::timer_2_event()
{
	//
}

void E64::hud_t::timer_3_event()
{
	//
}

void E64::hud_t::timer_4_event()
{
	//
}

void E64::hud_t::timer_5_event()
{
	//
}

void E64::hud_t::timer_6_event()
{
	//
}

void E64::hud_t::timer_7_event()
{
	//
}

void E64::hud_t::redraw()
{
	if ((stats_pos < 288) && (machine.mode == E64::RUNNING)) {
		stats_view->x_pos = 128;
		stats_view->y_pos = stats_pos;
		blitter->add_operation_draw_blit(stats_view);
	}
	if (stats_visible && (stats_pos > 256)) stats_pos--;
	if (!stats_visible && (stats_pos < 288)) stats_pos++;
	
	if (machine.mode == E64::PAUSED) {
		bar_64x2->x_pos = 0;
		bar_64x2->y_pos = -4;
		blitter->add_operation_draw_blit(bar_64x2);
		
		cpu_view->x_pos = 0;
		cpu_view->y_pos = 12;
		blitter->add_operation_draw_blit(cpu_view);
		
		bar_64x1->x_pos = 0;
		bar_64x1->y_pos = 28;
		blitter->add_operation_draw_blit(bar_64x1);
		
		disassembly_view->x_pos = 0;
		disassembly_view->y_pos = 36;
		blitter->add_operation_draw_blit(disassembly_view);
		
		stack_view->x_pos = 376;
		stack_view->y_pos = 36;
		blitter->add_operation_draw_blit(stack_view);
		
		bar_64x1->x_pos = 0;
		bar_64x1->y_pos = 100;
		blitter->add_operation_draw_blit(bar_64x1);
		
		other_info->x_pos = 0;
		other_info->y_pos = 108;
		blitter->add_operation_draw_blit(other_info);
		
		stats_view->x_pos = 256;
		stats_view->y_pos = 108;
		blitter->add_operation_draw_blit(stats_view);
		
		bar_64x1->x_pos = 0;
		bar_64x1->y_pos = 140;
		blitter->add_operation_draw_blit(bar_64x1);
		
		terminal->x_pos = 0;
		terminal->y_pos = 148;
		blitter->add_operation_draw_blit(terminal);
		
		bar_64x2->x_pos = 0;
		bar_64x2->y_pos = 276;
		blitter->add_operation_draw_blit(bar_64x2);
	} else {
		// machine running, check for recording activity
		if (machine.recording() && (recording_frame_counter & 0x80)) {
			recording_icon->x_pos = 468;
			recording_icon->y_pos = 276;
			blitter->add_operation_draw_blit(recording_icon);
		}
		recording_frame_counter -= 3;
	}
	
	if (notify_frame_counter) {
		int16_t temp_y_pos = 88 - abs(notify_frame_counter - notify_frames);
		if (temp_y_pos > 0 ) temp_y_pos = 0;
		notification->x_pos = 0;
		notification->y_pos = temp_y_pos;
		blitter->add_operation_draw_blit(notification);
		notify_frame_counter--;
	}
}

void E64::hud_t::process_command(char *buffer)
{
	bool have_prompt = true;
	
	char *token0, *token1;
	token0 = strtok(buffer, " ");
	
	if (token0 == NULL) {
		have_prompt = false;
		blitter->terminal_putchar(terminal->number, '\n');
		//terminal->terminal_putchar('\n');
	} else if (token0[0] == ':') {
		have_prompt = false;
		enter_monitor_line(buffer);
	} else if (token0[0] == ';') {
		have_prompt = false;
		enter_monitor_blit_line(buffer);
	} else if (strcmp(token0, "b") == 0) {
		token1 = strtok(NULL, " ");
		blitter->terminal_putchar(terminal->number, '\n');
		//terminal->terminal_putchar('\n');
		if (token1 == NULL) {
			uint16_t count = 0;
			for (int i=0; i< RAM_SIZE_CPU_VISIBLE; i++) {
				if (machine.cpu->breakpoint_array[i]) {
					blitter->terminal_printf(terminal->number, "%04x ", i);
					count++;
					if ((count % 4) == 0)
						blitter->terminal_putchar(terminal->number, '\n');
						//terminal->terminal_putchar('\n');
				}
			}
			if (count == 0) {
				blitter->terminal_puts(terminal->number, "no breakpoints");
				//terminal->puts("no breakpoints");
			}
		} else {
			uint32_t temp_32bit;
			if (hex_string_to_int(token1, &temp_32bit)) {
				temp_32bit &= (RAM_SIZE_CPU_VISIBLE - 1);
				machine.cpu->breakpoint_array[temp_32bit] =
					!machine.cpu->breakpoint_array[temp_32bit];
				blitter->terminal_printf(terminal->number, "breakpoint %s at $%04x",
						machine.cpu->breakpoint_array[temp_32bit] ? "set" : "cleared",
						temp_32bit);
			} else {
				blitter->terminal_puts(terminal->number, "error: invalid address\n");
			}
		}
	} else if (strcmp(token0, "bc") == 0 ) {
		blitter->terminal_puts(terminal->number, "\nclearing all breakpoints");
		machine.cpu->clear_breakpoints();
	} else if (strcmp(token0, "bm") == 0) {
		have_prompt = false;
		token1 = strtok(NULL, " ");

		uint8_t lines_remaining = terminal->lines_remaining();

		if (lines_remaining == 0) lines_remaining = 1;

		uint32_t blit_memory_location = 0x000000;

		if (token1 == NULL) {
			for (int i=0; i<lines_remaining; i++) {
				blitter->terminal_putchar(terminal->number, '\n');
				//terminal->terminal_putchar('\n');
				blit_memory_dump(blit_memory_location, 1);
				blit_memory_location = (blit_memory_location + 8) & 0xfffff8;
			}
		} else {
			if (!hex_string_to_int(token1, &blit_memory_location)) {
				blitter->terminal_putchar(terminal->number, '\n');
				//terminal->terminal_putchar('\n');
				blitter->terminal_puts(terminal->number, "error: invalid address\n");
			} else {
				for (int i=0; i<lines_remaining; i++) {
					blitter->terminal_putchar(terminal->number, '\n');
					//terminal->terminal_putchar('\n');
					blit_memory_dump(blit_memory_location & 0x00fffff8, 1);
					blit_memory_location = (blit_memory_location + 8) & 0xfffff8;
				}
			}
		}
	} else if (strcmp(token0, "c") == 0 ) {
		have_prompt = false;
		E64::sdl2_wait_until_enter_released();
		blitter->terminal_putchar(terminal->number, '\n');
		//terminal->terminal_putchar('\n');
		machine.flip_modes();
		
		/*
		 * This extra call ensures the keystates are nice when
		 * entering the machine again
		 */
		sdl2_process_events();
	} else if (strcmp(token0, "clear") == 0 ) {
		have_prompt = false;
		blitter->terminal_clear(terminal->number);
		//terminal->terminal_clear();
	} else if (strcmp(token0, "exit") == 0) {
		have_prompt = false;
		E64::sdl2_wait_until_enter_released();
		app_running = false;
	} else if (strcmp(token0, "m") == 0) {
		have_prompt = false;
		token1 = strtok(NULL, " ");
		
		uint8_t lines_remaining = terminal->lines_remaining();
		
		if (lines_remaining == 0) lines_remaining = 1;

		uint32_t temp_pc = machine.cpu->get_pc();
	
		if (token1 == NULL) {
			for (int i=0; i<lines_remaining; i++) {
				blitter->terminal_putchar(terminal->number, '\n');
				//terminal->terminal_putchar('\n');
				memory_dump(temp_pc, 1);
				temp_pc = (temp_pc + 8) & 0xffff;
			}
		} else {
			if (!hex_string_to_int(token1, &temp_pc)) {
				blitter->terminal_putchar(terminal->number, '\n');
				//terminal->terminal_putchar('\n');
				blitter->terminal_puts(terminal->number, "error: invalid address\n");
			} else {
				for (int i=0; i<lines_remaining; i++) {
					blitter->terminal_putchar(terminal->number, '\n');
					//terminal->terminal_putchar('\n');
					memory_dump(temp_pc & (RAM_SIZE_CPU_VISIBLE - 1), 1);
					temp_pc = (temp_pc + 8) & 0xffff;
				}
			}
		}
	} else if (strcmp(token0, "reset") == 0) {
		E64::sdl2_wait_until_enter_released();
		machine.reset();
	} else if (strcmp(token0, "timers") == 0) {
		for (int i=0; i<8; i++) {
			char text_buffer[64];
			machine.timer->status(text_buffer, i);
			blitter->terminal_puts(terminal->number, text_buffer);
		}
	} else if (strcmp(token0, "ver") == 0) {
		blitter->terminal_printf(terminal->number, "\nE64 (C)%i - version %i.%i (%i)", E64_YEAR, E64_MAJOR_VERSION, E64_MINOR_VERSION, E64_BUILD);
	} else {
		blitter->terminal_putchar(terminal->number, '\n');
		//terminal->terminal_putchar('\n');
		blitter->terminal_printf(terminal->number, "error: unknown command '%s'", token0);
	}
	if (have_prompt) {
		blitter->terminal_prompt(terminal->number);
		//terminal->prompt();
	}
}

void E64::hud_t::memory_dump(uint16_t address, int rows)
{
    address = address & 0xffff;  // only even addresses allowed
    
	for (int i=0; i<rows; i++ ) {
		uint16_t temp_address = address;
		blitter->terminal_printf(terminal->number, "\r:%04x ", temp_address);
		for (int i=0; i<8; i++) {
			blitter->terminal_printf(terminal->number, "%02x ", machine.mmu->read_memory_8(temp_address));
			temp_address++;
			temp_address &= RAM_SIZE_CPU_VISIBLE - 1;
		}
	
		terminal->foreground_color = GREEN_07;
		terminal->background_color = (GREEN_04 & 0x0fff) | 0xc000;
		
		temp_address = address;
		for (int i=0; i<8; i++) {
			uint8_t temp_byte = machine.mmu->read_memory_8(temp_address);
			blitter->terminal_putsymbol(terminal->number, temp_byte);
			temp_address++;
		}
		address += 8;
		address &= RAM_SIZE_CPU_VISIBLE - 1;
	
		terminal->foreground_color = GREEN_05;
		terminal->background_color = (GREEN_01 & 0x0fff) | 0xc000;
       
		for (int i=0; i<32; i++) {
			blitter->terminal_cursor_left(terminal->number);
		}
	}
}

void E64::hud_t::blit_memory_dump(uint32_t address, int rows)
{
    address = address & 0xfffff8;
    
	for (int i=0; i<rows; i++ ) {
		uint32_t temp_address = address;
		blitter->terminal_printf(terminal->number, "\r;%06x ", temp_address);
		for (int i=0; i<8; i++) {
			blitter->terminal_printf(terminal->number, "%02x ", machine.blitter->video_memory_read_8(temp_address));
			temp_address++;
		}

		terminal->foreground_color = GREEN_06;
		terminal->background_color = (GREEN_02 & 0x0fff) | 0xc000;
		
		temp_address = address;
		for (int i=0; i<8; i++) {
			uint8_t temp_byte = machine.blitter->video_memory_read_8(temp_address);
			blitter->terminal_putsymbol(terminal->number, temp_byte);
			temp_address++;
		}
		address += 8;
		address &= 0x00fffff8;
	
		terminal->foreground_color = GREEN_05;
		terminal->background_color = (GREEN_01 & 0x0fff) | 0xc000;
       
		for (int i=0; i<32; i++) {
			blitter->terminal_cursor_left(terminal->number);
		}
	}
}

void E64::hud_t::enter_monitor_line(char *buffer)
{
	uint32_t address;
	uint32_t arg0, arg1, arg2, arg3;
	uint32_t arg4, arg5, arg6, arg7;
    
	buffer[5]  = '\0';
	buffer[8]  = '\0';
	buffer[11] = '\0';
	buffer[14] = '\0';
	buffer[17] = '\0';
	buffer[20] = '\0';
	buffer[23] = '\0';
	buffer[26] = '\0';
	buffer[29] = '\0';
    
	if (!hex_string_to_int(&buffer[1], &address)) {
		blitter->terminal_putchar(terminal->number, '\r');
		blitter->terminal_cursor_right(terminal->number);
		blitter->terminal_puts(terminal->number, "????\n");
	} else if (!hex_string_to_int(&buffer[6], &arg0)) {
		blitter->terminal_putchar(terminal->number, '\r');
		for (int i=0; i<6; i++) blitter->terminal_cursor_right(terminal->number);
		blitter->terminal_puts(terminal->number, "??\n");
	} else if (!hex_string_to_int(&buffer[9], &arg1)) {
		blitter->terminal_putchar(terminal->number, '\r');
		for (int i=0; i<9; i++) blitter->terminal_cursor_right(terminal->number);
		blitter->terminal_puts(terminal->number, "??\n");
	} else if (!hex_string_to_int(&buffer[12], &arg2)) {
		blitter->terminal_putchar(terminal->number, '\r');
		for (int i=0; i<12; i++) blitter->terminal_cursor_right(terminal->number);
		blitter->terminal_puts(terminal->number, "??\n");
	} else if (!hex_string_to_int(&buffer[15], &arg3)) {
		blitter->terminal_putchar(terminal->number, '\r');
		for (int i=0; i<15; i++) blitter->terminal_cursor_right(terminal->number);
		blitter->terminal_puts(terminal->number, "??\n");
	} else if (!hex_string_to_int(&buffer[18], &arg4)) {
		blitter->terminal_putchar(terminal->number, '\r');
		for (int i=0; i<18; i++) blitter->terminal_cursor_right(terminal->number);
		blitter->terminal_puts(terminal->number, "??\n");
	} else if (!hex_string_to_int(&buffer[21], &arg5)) {
		blitter->terminal_putchar(terminal->number, '\r');
		for (int i=0; i<21; i++) blitter->terminal_cursor_right(terminal->number);
		blitter->terminal_puts(terminal->number, "??\n");
	} else if (!hex_string_to_int(&buffer[24], &arg6)) {
		blitter->terminal_putchar(terminal->number, '\r');
		for (int i=0; i<24; i++) blitter->terminal_cursor_right(terminal->number);
		blitter->terminal_puts(terminal->number, "??\n");
	} else if (!hex_string_to_int(&buffer[27], &arg7)) {
		blitter->terminal_putchar(terminal->number, '\r');
		for (int i=0; i<27; i++) blitter->terminal_cursor_right(terminal->number);
		blitter->terminal_puts(terminal->number, "??\n");
	} else {
		uint16_t original_address = address;
	
		arg0 &= 0xff;
		arg1 &= 0xff;
		arg2 &= 0xff;
		arg3 &= 0xff;
		arg4 &= 0xff;
		arg5 &= 0xff;
		arg6 &= 0xff;
		arg7 &= 0xff;
	
		machine.mmu->write_memory_8(address, (uint8_t)arg0); address +=1; address &= 0xffff;
		machine.mmu->write_memory_8(address, (uint8_t)arg1); address +=1; address &= 0xffff;
		machine.mmu->write_memory_8(address, (uint8_t)arg2); address +=1; address &= 0xffff;
		machine.mmu->write_memory_8(address, (uint8_t)arg3); address +=1; address &= 0xffff;
		machine.mmu->write_memory_8(address, (uint8_t)arg4); address +=1; address &= 0xffff;
		machine.mmu->write_memory_8(address, (uint8_t)arg5); address +=1; address &= 0xffff;
		machine.mmu->write_memory_8(address, (uint8_t)arg6); address +=1; address &= 0xffff;
		machine.mmu->write_memory_8(address, (uint8_t)arg7); address +=1; address &= 0xffff;
		
		blitter->terminal_putchar(terminal->number, '\r');
		//terminal->terminal_putchar('\r');
	
		memory_dump(original_address, 1);
	
		original_address += 8;
		original_address &= 0xffff;
		blitter->terminal_printf(terminal->number, "\n:%04x ", original_address);
	}
}

void E64::hud_t::enter_monitor_blit_line(char *buffer)
{
	uint32_t address;
	uint32_t arg0, arg1, arg2, arg3;
	uint32_t arg4, arg5, arg6, arg7;
    
	buffer[7]  = '\0';
	buffer[10]  = '\0';
	buffer[13] = '\0';
	buffer[16] = '\0';
	buffer[19] = '\0';
	buffer[22] = '\0';
	buffer[25] = '\0';
	buffer[28] = '\0';
	buffer[31] = '\0';
    
	if (!hex_string_to_int(&buffer[1], &address)) {
		blitter->terminal_putchar(terminal->number, '\r');
		blitter->terminal_cursor_right(terminal->number);
		blitter->terminal_puts(terminal->number, "??????\n");
	} else if (!hex_string_to_int(&buffer[8], &arg0)) {
		blitter->terminal_putchar(terminal->number, '\r');
		for (int i=0; i<8; i++) blitter->terminal_cursor_right(terminal->number);
		blitter->terminal_puts(terminal->number, "??\n");
	} else if (!hex_string_to_int(&buffer[11], &arg1)) {
		blitter->terminal_putchar(terminal->number, '\r');
		for (int i=0; i<11; i++) blitter->terminal_cursor_right(terminal->number);
		blitter->terminal_puts(terminal->number, "??\n");
	} else if (!hex_string_to_int(&buffer[14], &arg2)) {
		blitter->terminal_putchar(terminal->number, '\r');
		for (int i=0; i<14; i++) blitter->terminal_cursor_right(terminal->number);
		blitter->terminal_puts(terminal->number, "??\n");
	} else if (!hex_string_to_int(&buffer[17], &arg3)) {
		blitter->terminal_putchar(terminal->number, '\r');
		for (int i=0; i<17; i++) blitter->terminal_cursor_right(terminal->number);
		blitter->terminal_puts(terminal->number, "??\n");
	} else if (!hex_string_to_int(&buffer[20], &arg4)) {
		blitter->terminal_putchar(terminal->number, '\r');
		for (int i=0; i<20; i++) blitter->terminal_cursor_right(terminal->number);
		blitter->terminal_puts(terminal->number, "??\n");
	} else if (!hex_string_to_int(&buffer[23], &arg5)) {
		blitter->terminal_putchar(terminal->number, '\r');
		for (int i=0; i<23; i++) blitter->terminal_cursor_right(terminal->number);
		blitter->terminal_puts(terminal->number, "??\n");
	} else if (!hex_string_to_int(&buffer[26], &arg6)) {
		blitter->terminal_putchar(terminal->number, '\r');
		for (int i=0; i<26; i++) blitter->terminal_cursor_right(terminal->number);
		blitter->terminal_puts(terminal->number, "??\n");
	} else if (!hex_string_to_int(&buffer[29], &arg7)) {
		blitter->terminal_putchar(terminal->number, '\r');
		for (int i=0; i<29; i++) blitter->terminal_cursor_right(terminal->number);
		blitter->terminal_puts(terminal->number, "??\n");
	} else {
		uint32_t original_address = address;
	
		arg0 &= 0xff;
		arg1 &= 0xff;
		arg2 &= 0xff;
		arg3 &= 0xff;
		arg4 &= 0xff;
		arg5 &= 0xff;
		arg6 &= 0xff;
		arg7 &= 0xff;
	
		machine.blitter->video_memory_write_8(address, (uint8_t)arg0); address +=1; address &= 0xffffff;
		machine.blitter->video_memory_write_8(address, (uint8_t)arg1); address +=1; address &= 0xffffff;
		machine.blitter->video_memory_write_8(address, (uint8_t)arg2); address +=1; address &= 0xffffff;
		machine.blitter->video_memory_write_8(address, (uint8_t)arg3); address +=1; address &= 0xffffff;
		machine.blitter->video_memory_write_8(address, (uint8_t)arg4); address +=1; address &= 0xffffff;
		machine.blitter->video_memory_write_8(address, (uint8_t)arg5); address +=1; address &= 0xffffff;
		machine.blitter->video_memory_write_8(address, (uint8_t)arg6); address +=1; address &= 0xffffff;
		machine.blitter->video_memory_write_8(address, (uint8_t)arg7); address +=1; address &= 0xffffff;
		
		blitter->terminal_putchar(terminal->number, '\r');
	
		blit_memory_dump(original_address, 1);
	
		original_address += 8;
		original_address &= 0xffffff;
		blitter->terminal_printf(terminal->number, "\n;%06x ", original_address);
	}
}

void E64::hud_t::show_notification(const char *format, ...)
{
	notify_frame_counter = notify_frames;
	
	if (blitter) {
		blitter->terminal_clear(notification->number);
	}
	
	char buffer[1024];
	va_list args;
	va_start(args, format);
	vsnprintf(buffer, 1024, format, args);
	va_end(args);

	if (blitter) {
		blitter->terminal_printf(notification->number, "\n\n\n%s", buffer);
	}
}

void E64::hud_t::toggle_stats()
{
	stats_visible = !stats_visible;
}
