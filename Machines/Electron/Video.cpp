//
//  Video.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/12/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "Video.hpp"

#include <cstring>

using namespace Electron;

#define graphics_line(v)	((((v) >> 7) - first_graphics_line + field_divider_line) % field_divider_line)
#define graphics_column(v)	((((v) & 127) - first_graphics_cycle + 128) & 127)

namespace {
	constexpr int cycles_per_line = 128;
	constexpr int lines_per_frame = 625;
	constexpr int cycles_per_frame = lines_per_frame * cycles_per_line;
	constexpr int crt_cycles_multiplier = 8;
	constexpr int crt_cycles_per_line = crt_cycles_multiplier * cycles_per_line;

	constexpr int field_divider_line = 312;	// i.e. the line, simultaneous with which, the first field's sync ends. So if
											// the first line with pixels in field 1 is the 20th in the frame, the first line
											// with pixels in field 2 will be 20+field_divider_line
	constexpr int first_graphics_line = 31;
	constexpr int first_graphics_cycle = 33;

	constexpr int display_end_interrupt_line = 256;

	constexpr int real_time_clock_interrupt_1 = 16704;
	constexpr int real_time_clock_interrupt_2 = 56704;
	constexpr int display_end_interrupt_1 = (first_graphics_line + display_end_interrupt_line)*cycles_per_line;
	constexpr int display_end_interrupt_2 = (first_graphics_line + field_divider_line + display_end_interrupt_line)*cycles_per_line;
}

// MARK: - Lifecycle

VideoOutput::VideoOutput(uint8_t *memory) :
	ram_(memory),
	crt_(crt_cycles_per_line,
		1,
		Outputs::Display::Type::PAL50,
		Outputs::Display::InputDataType::Red1Green1Blue1) {
	memset(palette_, 0xf, sizeof(palette_));
	setup_screen_map();
	setup_base_address();

	// TODO: as implied below, I've introduced a clock's latency into the graphics pipeline somehow. Investigate.
	crt_.set_visible_area(crt_.get_rect_for_area(first_graphics_line - 1, 256, (first_graphics_cycle+1) * crt_cycles_multiplier, 80 * crt_cycles_multiplier, 4.0f / 3.0f));
}

void VideoOutput::set_scan_target(Outputs::Display::ScanTarget *scan_target) {
	crt_.set_scan_target(scan_target);
}

Outputs::Display::ScanStatus VideoOutput::get_scaled_scan_status() const {
	return crt_.get_scaled_scan_status() / float(crt_cycles_multiplier);
}

void VideoOutput::set_display_type(Outputs::Display::DisplayType display_type) {
	crt_.set_display_type(display_type);
}

// MARK: - Display update methods

void VideoOutput::start_pixel_line() {
	current_pixel_line_ = (current_pixel_line_+1)&255;
	if(!current_pixel_line_) {
		start_line_address_ = start_screen_address_;
		current_character_row_ = 0;
		is_blank_line_ = false;
	} else {
		bool mode_has_blank_lines = (screen_mode_ == 6) || (screen_mode_ == 3);
		is_blank_line_ = (mode_has_blank_lines && ((current_character_row_ > 7 && current_character_row_ < 10) || (current_pixel_line_ > 249)));

		if(!is_blank_line_) {
			start_line_address_++;

			if(current_character_row_ > 7) {
				start_line_address_ += ((screen_mode_ < 4) ? 80 : 40) * 8 - 8;
				current_character_row_ = 0;
			}
		}
	}
	current_screen_address_ = start_line_address_;
	current_pixel_column_ = 0;
	initial_output_target_ = current_output_target_ = nullptr;
}

void VideoOutput::end_pixel_line() {
	const int data_length = int(current_output_target_ - initial_output_target_);
	if(data_length) {
		crt_.output_data(data_length * current_output_divider_, size_t(data_length));
	}
	current_character_row_++;
}

void VideoOutput::output_pixels(int number_of_cycles) {
	if(!number_of_cycles) return;

	if(is_blank_line_) {
		crt_.output_blank(number_of_cycles * crt_cycles_multiplier);
	} else {
		int divider = 1;
		switch(screen_mode_) {
			case 0: case 3: divider = 1; break;
			case 1: case 4: case 6: divider = 2; break;
			case 2: case 5: divider = 4; break;
		}

		if(!initial_output_target_ || divider != current_output_divider_) {
			const int data_length = int(current_output_target_ - initial_output_target_);
			if(data_length) {
				crt_.output_data(data_length * current_output_divider_, size_t(data_length));
			}
			current_output_divider_ = divider;
			initial_output_target_ = current_output_target_ = crt_.begin_data(size_t(640 / current_output_divider_), size_t(8 / divider));
		}

#define get_pixel()	\
				if(current_screen_address_&32768) {\
					current_screen_address_ = (screen_mode_base_address_ + current_screen_address_)&32767;\
				}\
				last_pixel_byte_ = ram_[current_screen_address_];\
				current_screen_address_ = current_screen_address_+8

		switch(screen_mode_) {
			case 0: case 3:
				if(initial_output_target_) {
					while(number_of_cycles--) {
						get_pixel();
						*reinterpret_cast<uint64_t *>(current_output_target_) = palette_tables_.eighty1bpp[last_pixel_byte_];
						current_output_target_ += 8;
						current_pixel_column_++;
					}
				} else current_output_target_ += 8*number_of_cycles;
			break;

			case 1:
				if(initial_output_target_) {
					while(number_of_cycles--) {
						get_pixel();
						*reinterpret_cast<uint32_t *>(current_output_target_) = palette_tables_.eighty2bpp[last_pixel_byte_];
						current_output_target_ += 4;
						current_pixel_column_++;
					}
				} else current_output_target_ += 4*number_of_cycles;
			break;

			case 2:
				if(initial_output_target_) {
					while(number_of_cycles--) {
						get_pixel();
						*reinterpret_cast<uint16_t *>(current_output_target_) = palette_tables_.eighty4bpp[last_pixel_byte_];
						current_output_target_ += 2;
						current_pixel_column_++;
					}
				} else current_output_target_ += 2*number_of_cycles;
			break;

			case 4: case 6:
				if(initial_output_target_) {
					if(current_pixel_column_&1) {
						last_pixel_byte_ <<= 4;
						*reinterpret_cast<uint32_t *>(current_output_target_) = palette_tables_.forty1bpp[last_pixel_byte_];
						current_output_target_ += 4;

						number_of_cycles--;
						current_pixel_column_++;
					}
					while(number_of_cycles > 1) {
						get_pixel();
						*reinterpret_cast<uint32_t *>(current_output_target_) = palette_tables_.forty1bpp[last_pixel_byte_];
						current_output_target_ += 4;

						last_pixel_byte_ <<= 4;
						*reinterpret_cast<uint32_t *>(current_output_target_) = palette_tables_.forty1bpp[last_pixel_byte_];
						current_output_target_ += 4;

						number_of_cycles -= 2;
						current_pixel_column_+=2;
					}
					if(number_of_cycles) {
						get_pixel();
						*reinterpret_cast<uint32_t *>(current_output_target_) = palette_tables_.forty1bpp[last_pixel_byte_];
						current_output_target_ += 4;
						current_pixel_column_++;
					}
				} else current_output_target_ += 4 * number_of_cycles;
			break;

			case 5:
				if(initial_output_target_) {
					if(current_pixel_column_&1) {
						last_pixel_byte_ <<= 2;
						*reinterpret_cast<uint16_t *>(current_output_target_) = palette_tables_.forty2bpp[last_pixel_byte_];
						current_output_target_ += 2;

						number_of_cycles--;
						current_pixel_column_++;
					}
					while(number_of_cycles > 1) {
						get_pixel();
						*reinterpret_cast<uint16_t *>(current_output_target_) = palette_tables_.forty2bpp[last_pixel_byte_];
						current_output_target_ += 2;

						last_pixel_byte_ <<= 2;
						*reinterpret_cast<uint16_t *>(current_output_target_) = palette_tables_.forty2bpp[last_pixel_byte_];
						current_output_target_ += 2;

						number_of_cycles -= 2;
						current_pixel_column_+=2;
					}
					if(number_of_cycles) {
						get_pixel();
						*reinterpret_cast<uint16_t *>(current_output_target_) = palette_tables_.forty2bpp[last_pixel_byte_];
						current_output_target_ += 2;
						current_pixel_column_++;
					}
				} else current_output_target_ += 2*number_of_cycles;
			break;
		}

#undef get_pixel
	}
}

void VideoOutput::run_for(const Cycles cycles) {
	int number_of_cycles = int(cycles.as_integral());
	output_position_ = (output_position_ + number_of_cycles) % cycles_per_frame;
	while(number_of_cycles) {
		int draw_action_length = screen_map_[screen_map_pointer_].length;
		int time_left_in_action = std::min(number_of_cycles, draw_action_length - cycles_into_draw_action_);
		if(screen_map_[screen_map_pointer_].type == DrawAction::Pixels) output_pixels(time_left_in_action);

		number_of_cycles -= time_left_in_action;
		cycles_into_draw_action_ += time_left_in_action;
		if(cycles_into_draw_action_ == draw_action_length) {
			switch(screen_map_[screen_map_pointer_].type) {
				case DrawAction::Sync:			crt_.output_sync(draw_action_length * crt_cycles_multiplier);					break;
				case DrawAction::ColourBurst:	crt_.output_default_colour_burst(draw_action_length * crt_cycles_multiplier);	break;
				case DrawAction::Blank:			crt_.output_blank(draw_action_length * crt_cycles_multiplier);					break;
				case DrawAction::Pixels:		end_pixel_line();																break;
			}
			screen_map_pointer_ = (screen_map_pointer_ + 1) % screen_map_.size();
			cycles_into_draw_action_ = 0;
			if(screen_map_[screen_map_pointer_].type == DrawAction::Pixels) start_pixel_line();
		}
	}
}

// MARK: - Register hub

void VideoOutput::write(int address, uint8_t value) {
	switch(address & 0xf) {
		case 0x02:
			start_screen_address_ = (start_screen_address_ & 0xfe00) | static_cast<uint16_t>((value & 0xe0) << 1);
			if(!start_screen_address_) start_screen_address_ |= 0x8000;
		break;
		case 0x03:
			start_screen_address_ = (start_screen_address_ & 0x01ff) | static_cast<uint16_t>((value & 0x3f) << 9);
			if(!start_screen_address_) start_screen_address_ |= 0x8000;
		break;
		case 0x07: {
			// update screen mode
			uint8_t new_screen_mode = (value >> 3)&7;
			if(new_screen_mode == 7) new_screen_mode = 4;
			if(new_screen_mode != screen_mode_) {
				screen_mode_ = new_screen_mode;
				setup_base_address();
			}
		}
		break;
		case 0x08: case 0x09: case 0x0a: case 0x0b:
		case 0x0c: case 0x0d: case 0x0e: case 0x0f: {
			constexpr int registers[4][4] = {
				{10, 8, 2, 0},
				{14, 12, 6, 4},
				{15, 13, 7, 5},
				{11, 9, 3, 1},
			};
			const int index = (address >> 1)&3;
			const uint8_t colour = ~value;
			if(address&1) {
				palette_[registers[index][0]]	= (palette_[registers[index][0]]&3)	| ((colour >> 1)&4);
				palette_[registers[index][1]]	= (palette_[registers[index][1]]&3)	| ((colour >> 0)&4);
				palette_[registers[index][2]]	= (palette_[registers[index][2]]&3)	| ((colour << 1)&4);
				palette_[registers[index][3]]	= (palette_[registers[index][3]]&3)	| ((colour << 2)&4);

				palette_[registers[index][2]]	= (palette_[registers[index][2]]&5)	| ((colour >> 4)&2);
				palette_[registers[index][3]]	= (palette_[registers[index][3]]&5)	| ((colour >> 3)&2);
			} else {
				palette_[registers[index][0]]	= (palette_[registers[index][0]]&6)	| ((colour >> 7)&1);
				palette_[registers[index][1]]	= (palette_[registers[index][1]]&6)	| ((colour >> 6)&1);
				palette_[registers[index][2]]	= (palette_[registers[index][2]]&6)	| ((colour >> 5)&1);
				palette_[registers[index][3]]	= (palette_[registers[index][3]]&6)	| ((colour >> 4)&1);

				palette_[registers[index][0]]	= (palette_[registers[index][0]]&5)	| ((colour >> 2)&2);
				palette_[registers[index][1]]	= (palette_[registers[index][1]]&5)	| ((colour >> 1)&2);
			}

			// regenerate all palette tables for now
			for(int byte = 0; byte < 256; byte++) {
				uint8_t *target = reinterpret_cast<uint8_t *>(&palette_tables_.forty1bpp[byte]);
				target[0] = palette_[(byte&0x80) >> 4];
				target[1] = palette_[(byte&0x40) >> 3];
				target[2] = palette_[(byte&0x20) >> 2];
				target[3] = palette_[(byte&0x10) >> 1];

				target = reinterpret_cast<uint8_t *>(&palette_tables_.eighty2bpp[byte]);
				target[0] = palette_[((byte&0x80) >> 4) | ((byte&0x08) >> 2)];
				target[1] = palette_[((byte&0x40) >> 3) | ((byte&0x04) >> 1)];
				target[2] = palette_[((byte&0x20) >> 2) | ((byte&0x02) >> 0)];
				target[3] = palette_[((byte&0x10) >> 1) | ((byte&0x01) << 1)];

				target = reinterpret_cast<uint8_t *>(&palette_tables_.eighty1bpp[byte]);
				target[0] = palette_[(byte&0x80) >> 4];
				target[1] = palette_[(byte&0x40) >> 3];
				target[2] = palette_[(byte&0x20) >> 2];
				target[3] = palette_[(byte&0x10) >> 1];
				target[4] = palette_[(byte&0x08) >> 0];
				target[5] = palette_[(byte&0x04) << 1];
				target[6] = palette_[(byte&0x02) << 2];
				target[7] = palette_[(byte&0x01) << 3];

				target = reinterpret_cast<uint8_t *>(&palette_tables_.forty2bpp[byte]);
				target[0] = palette_[((byte&0x80) >> 4) | ((byte&0x08) >> 2)];
				target[1] = palette_[((byte&0x40) >> 3) | ((byte&0x04) >> 1)];

				target = reinterpret_cast<uint8_t *>(&palette_tables_.eighty4bpp[byte]);
				target[0] = palette_[((byte&0x80) >> 4) | ((byte&0x20) >> 3) | ((byte&0x08) >> 2) | ((byte&0x02) >> 1)];
				target[1] = palette_[((byte&0x40) >> 3) | ((byte&0x10) >> 2) | ((byte&0x04) >> 1) | ((byte&0x01) >> 0)];
			}
		}
		break;
	}
}

void VideoOutput::setup_base_address() {
	switch(screen_mode_) {
		case 0: case 1: case 2: screen_mode_base_address_ = 0x3000; break;
		case 3: screen_mode_base_address_ = 0x4000; break;
		case 4: case 5: screen_mode_base_address_ = 0x5800; break;
		case 6: screen_mode_base_address_ = 0x6000; break;
	}
}

// MARK: - Interrupts

VideoOutput::Interrupt VideoOutput::get_next_interrupt() {
	VideoOutput::Interrupt interrupt;

	if(output_position_ < real_time_clock_interrupt_1) {
		interrupt.cycles = real_time_clock_interrupt_1 - output_position_;
		interrupt.interrupt = RealTimeClock;
		return interrupt;
	}

	if(output_position_ < display_end_interrupt_1) {
		interrupt.cycles = display_end_interrupt_1 - output_position_;
		interrupt.interrupt = DisplayEnd;
		return interrupt;
	}

	if(output_position_ < real_time_clock_interrupt_2) {
		interrupt.cycles = real_time_clock_interrupt_2 - output_position_;
		interrupt.interrupt = RealTimeClock;
		return interrupt;
	}

	if(output_position_ < display_end_interrupt_2) {
		interrupt.cycles = display_end_interrupt_2 - output_position_;
		interrupt.interrupt = DisplayEnd;
		return interrupt;
	}

	interrupt.cycles = real_time_clock_interrupt_1 + cycles_per_frame - output_position_;
	interrupt.interrupt = RealTimeClock;
	return interrupt;
}

// MARK: - RAM timing and access information

unsigned int VideoOutput::get_cycles_until_next_ram_availability(int from_time) {
	unsigned int result = 0;
	int position = (output_position_ + from_time) % cycles_per_frame;

	// Apply the standard cost of aligning to the available 1Mhz of RAM bandwidth.
	result += 1 + (position&1);

	// In Modes 0-3 there is also a complete block on any access while pixels are being fetched.
	if(screen_mode_ < 4) {
		const int current_column = graphics_column(position + (position&1));
		int current_line = graphics_line(position);
		if(current_column < 80 && current_line < 256) {
			// Mode 3 is a further special case: in 'every ten line block', the final two aren't painted,
			// so the CPU is allowed access. But the offset of the ten-line blocks depends on when the
			// user switched into Mode 3, so that needs to be calculated relative to current output.
			if(screen_mode_ == 3) {
				// Get the line the display was on.
				int output_position_line = graphics_line(output_position_);

				int implied_row;
				if(current_line >= output_position_line) {
					// Get the number of lines since then if still in the same frame.
					int lines_since_output_position = current_line - output_position_line;

					// Therefore get the character row at the proposed time, modulo 10.
					implied_row = (current_character_row_ + lines_since_output_position) % 10;
				} else {
					// If the frame has rolled over, the implied row is just related to the current line.
					implied_row = current_line % 10;
				}

				// Mode 3 ends after 250 lines, not the usual 256.
				if(implied_row < 8 && current_line < 250) result += static_cast<unsigned int>(80 - current_column);
			}
			else result += static_cast<unsigned int>(80 - current_column);
		}
	}
	return result;
}

VideoOutput::Range VideoOutput::get_memory_access_range() {
	// This can't be more specific than this without applying a lot more thought because of mixed modes:
	// suppose a program runs half the screen in an 80-column mode then switches to 40 columns. Then the
	// real end address will be at 128*80 + 128*40 after the original base, subject to wrapping that depends
	// on where the overflow occurred. Assuming accesses may run from the lowest possible position through to
	// the end of RAM is good enough for 95% of use cases however.
	VideoOutput::Range range;
	range.low_address = std::min(start_screen_address_, screen_mode_base_address_);
	range.high_address = 0x8000;
	return range;
}

// MARK: - The screen map

void VideoOutput::setup_screen_map() {
	/*

		Odd field:					Even field:

		|--S--|						   -S-|
		|--S--|						|--S--|
		|-S-B-|	= 3					|--S--| = 2.5
		|--B--|						|--B--|
		|--P--|						|--P--|
		|--B--| = 312				|--B--| = 312.5
		|-B-

	*/
	for(int c = 0; c < 2; c++) {
		if(c&1) {
			screen_map_.emplace_back(DrawAction::Sync, (cycles_per_line * 5) >> 1);
			screen_map_.emplace_back(DrawAction::Blank, cycles_per_line >> 1);
		} else {
			screen_map_.emplace_back(DrawAction::Blank, cycles_per_line >> 1);
			screen_map_.emplace_back(DrawAction::Sync, (cycles_per_line * 5) >> 1);
		}
		for(int l = 0; l < first_graphics_line - 3; l++) emplace_blank_line();
		for(int l = 0; l < 256; l++) emplace_pixel_line();
		for(int l = 256 + first_graphics_line; l < 312; l++) emplace_blank_line();
		if(c&1) emplace_blank_line();
	}
}

void VideoOutput::emplace_blank_line() {
	screen_map_.emplace_back(DrawAction::Sync, 9);
	screen_map_.emplace_back(DrawAction::ColourBurst, 24 - 9);
	screen_map_.emplace_back(DrawAction::Blank, 128 - 24);
}

void VideoOutput::emplace_pixel_line() {
	// output format is:
	// 9 cycles: sync
	// ... to 24 cycles: colour burst
	// ... to first_graphics_cycle: blank
	// ... for 80 cycles: pixels
	// ... until end of line: blank
	screen_map_.emplace_back(DrawAction::Sync, 9);
	screen_map_.emplace_back(DrawAction::ColourBurst, 24 - 9);
	screen_map_.emplace_back(DrawAction::Blank, first_graphics_cycle - 24);
	screen_map_.emplace_back(DrawAction::Pixels, 80);
	screen_map_.emplace_back(DrawAction::Blank, 48 - first_graphics_cycle);
}
