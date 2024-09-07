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

VideoOutput::VideoOutput(const uint8_t *memory) :
	ram_(memory),
	crt_(crt_cycles_per_line,
		1,
		Outputs::Display::Type::PAL50,
		Outputs::Display::InputDataType::Red1Green1Blue1) {
	memset(palette_, 0xf, sizeof(palette_));

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

Outputs::Display::DisplayType VideoOutput::get_display_type() const {
	return crt_.get_display_type();
}

// MARK: - Display update methods

//void VideoOutput::start_pixel_line() {
//	current_pixel_line_ = (current_pixel_line_+1)&255;
//	if(!current_pixel_line_) {
//		start_line_address_ = start_screen_address_;
//		current_character_row_ = 0;
//		is_blank_line_ = false;
//	} else {
//		bool mode_has_blank_lines = (screen_mode_ == 6) || (screen_mode_ == 3);
//		is_blank_line_ = (mode_has_blank_lines && ((current_character_row_ > 7 && current_character_row_ < 10) || (current_pixel_line_ > 249)));
//
//		if(!is_blank_line_) {
//			start_line_address_++;
//
//			if(current_character_row_ > 7) {
//				start_line_address_ += ((screen_mode_ < 4) ? 80 : 40) * 8 - 8;
//				current_character_row_ = 0;
//			}
//		}
//	}
//	current_screen_address_ = start_line_address_;
//	current_pixel_column_ = 0;
//	initial_output_target_ = current_output_target_ = nullptr;
//}
//
//void VideoOutput::end_pixel_line() {
//	const int data_length = int(current_output_target_ - initial_output_target_);
//	if(data_length) {
//		crt_.output_data(data_length * current_output_divider_, size_t(data_length));
//	}
//	current_character_row_++;
//}
//
//void VideoOutput::output_pixels(int number_of_cycles) {
//	if(!number_of_cycles) return;
//
//	if(is_blank_line_) {
//		crt_.output_blank(number_of_cycles * crt_cycles_multiplier);
//	} else {
//		int divider = 1;
//		switch(screen_mode_) {
//			case 0: case 3: divider = 1; break;
//			case 1: case 4: case 6: divider = 2; break;
//			case 2: case 5: divider = 4; break;
//		}
//
//		if(!initial_output_target_ || divider != current_output_divider_) {
//			const int data_length = int(current_output_target_ - initial_output_target_);
//			if(data_length) {
//				crt_.output_data(data_length * current_output_divider_, size_t(data_length));
//			}
//			current_output_divider_ = divider;
//			initial_output_target_ = current_output_target_ = crt_.begin_data(size_t(640 / current_output_divider_), size_t(8 / divider));
//		}
//
//#define get_pixel()	\
//				if(current_screen_address_&32768) {\
//					current_screen_address_ = (screen_mode_base_address_ + current_screen_address_)&32767;\
//				}\
//				last_pixel_byte_ = ram_[current_screen_address_];\
//				current_screen_address_ = current_screen_address_+8
//
//		switch(screen_mode_) {
//			case 0: case 3:
//				if(initial_output_target_) {
//					while(number_of_cycles--) {
//						get_pixel();
//						*reinterpret_cast<uint64_t *>(current_output_target_) = palette_tables_.eighty1bpp[last_pixel_byte_];
//						current_output_target_ += 8;
//						current_pixel_column_++;
//					}
//				} else current_output_target_ += 8*number_of_cycles;
//			break;
//
//			case 1:
//				if(initial_output_target_) {
//					while(number_of_cycles--) {
//						get_pixel();
//						*reinterpret_cast<uint32_t *>(current_output_target_) = palette_tables_.eighty2bpp[last_pixel_byte_];
//						current_output_target_ += 4;
//						current_pixel_column_++;
//					}
//				} else current_output_target_ += 4*number_of_cycles;
//			break;
//
//			case 2:
//				if(initial_output_target_) {
//					while(number_of_cycles--) {
//						get_pixel();
//						*reinterpret_cast<uint16_t *>(current_output_target_) = palette_tables_.eighty4bpp[last_pixel_byte_];
//						current_output_target_ += 2;
//						current_pixel_column_++;
//					}
//				} else current_output_target_ += 2*number_of_cycles;
//			break;
//
//			case 4: case 6:
//				if(initial_output_target_) {
//					if(current_pixel_column_&1) {
//						last_pixel_byte_ <<= 4;
//						*reinterpret_cast<uint32_t *>(current_output_target_) = palette_tables_.forty1bpp[last_pixel_byte_];
//						current_output_target_ += 4;
//
//						number_of_cycles--;
//						current_pixel_column_++;
//					}
//					while(number_of_cycles > 1) {
//						get_pixel();
//						*reinterpret_cast<uint32_t *>(current_output_target_) = palette_tables_.forty1bpp[last_pixel_byte_];
//						current_output_target_ += 4;
//
//						last_pixel_byte_ <<= 4;
//						*reinterpret_cast<uint32_t *>(current_output_target_) = palette_tables_.forty1bpp[last_pixel_byte_];
//						current_output_target_ += 4;
//
//						number_of_cycles -= 2;
//						current_pixel_column_+=2;
//					}
//					if(number_of_cycles) {
//						get_pixel();
//						*reinterpret_cast<uint32_t *>(current_output_target_) = palette_tables_.forty1bpp[last_pixel_byte_];
//						current_output_target_ += 4;
//						current_pixel_column_++;
//					}
//				} else current_output_target_ += 4 * number_of_cycles;
//			break;
//
//			case 5:
//				if(initial_output_target_) {
//					if(current_pixel_column_&1) {
//						last_pixel_byte_ <<= 2;
//						*reinterpret_cast<uint16_t *>(current_output_target_) = palette_tables_.forty2bpp[last_pixel_byte_];
//						current_output_target_ += 2;
//
//						number_of_cycles--;
//						current_pixel_column_++;
//					}
//					while(number_of_cycles > 1) {
//						get_pixel();
//						*reinterpret_cast<uint16_t *>(current_output_target_) = palette_tables_.forty2bpp[last_pixel_byte_];
//						current_output_target_ += 2;
//
//						last_pixel_byte_ <<= 2;
//						*reinterpret_cast<uint16_t *>(current_output_target_) = palette_tables_.forty2bpp[last_pixel_byte_];
//						current_output_target_ += 2;
//
//						number_of_cycles -= 2;
//						current_pixel_column_+=2;
//					}
//					if(number_of_cycles) {
//						get_pixel();
//						*reinterpret_cast<uint16_t *>(current_output_target_) = palette_tables_.forty2bpp[last_pixel_byte_];
//						current_output_target_ += 2;
//						current_pixel_column_++;
//					}
//				} else current_output_target_ += 2*number_of_cycles;
//			break;
//		}
//
//#undef get_pixel
//	}
//}

uint8_t VideoOutput::run_for(const Cycles cycles) {
	uint8_t interrupts{};

	int number_of_cycles = cycles.as<int>();
	while(number_of_cycles--) {
		// Horizontal and vertical counter updates.
		const bool is_v_end = v_count == v_total();
		h_count += 8;
		if(h_count == h_total) {
			h_count = 0;
			++v_count;

			if(is_v_end) {
				v_count = 0;
				field = !field;
			}
		}

		// Test for interrupts.
		if(v_count == v_rtc && ((!field && !h_count) || (field && h_count == h_half))) {
			interrupts |= static_cast<uint8_t>(Interrupt::RealTimeClock);
		}
		if(h_count == hsync_start && ((v_count == v_disp_gph && !mode_text) or (v_count == v_disp_txt && mode_text))) {
			interrupts |= static_cast<uint8_t>(Interrupt::DisplayEnd);
		}

		// Update syncs.
		if(!field) {
			if(!h_count && v_count == vsync_start) {
				vsync_int = true;
			} else if(h_count == h_half && v_count == vsync_end) {
				vsync_int = false;
			}
		} else {
			if(h_count == h_half && v_count == vsync_start) {
				vsync_int = true;
			} else if(!h_count && v_count == vsync_end + 1) {
				vsync_int = false;
			}
		}
		
		const auto h_sync_last = hsync_int;
		if(h_count == hsync_start) {
			hsync_int = true;
		} else if(h_count == hsync_end) {
			hsync_int = false;
		}
		
		// Update character row on the trailing edge of hsync.
		if(h_count == hsync_end) {
			if(is_v_end) {
				char_row = 0;
			} else {
				char_row = last_line() ? 0 : char_row + 1;
			}
		}

		// Disable the top bit of the char_row counter outside of text mode.
		if(!mode_text) {
			char_row &= 7;
		}

		// Latch video address at frame start.
		if(h_count == h_reset_addr && is_v_end) {
			row_addr = byte_addr = screen_base;
		}
		
		// Copy byte_addr back into row_addr if a new character row has begun.
		if(hsync_int) {
			if(last_line()) {
				row_addr = byte_addr;
			} else {
				byte_addr = row_addr;
			}
		}

		// Increment the byte address across the line.
		// (slghtly pained logic here because the input clock is still at the pixel rate, not the byte rate)
		if(h_count < h_active) {
			if(
				(!mode_40 && !(h_count & 0x7)) ||
				(mode_40 && ((h_count & 0xf) == 0x8))
			) {
				byte_addr += 8;

				if(!(byte_addr & 0b0111'1000'0000'0000)) {
					byte_addr = mode_base | (byte_addr & 0x0000'0111'1111'1111);
				}
			}
		}	
	}

	return interrupts;
}

// MARK: - Register hub

void VideoOutput::write(int address, uint8_t value) {
	switch(address & 0xf) {
		case 0x02:
		screen_base = 
			(screen_base & 0b0111'1110'0000'0000) |
			((value << 1) & 0b0000'0001'1100'0000);
		break;
		case 0x03:
			screen_base =
				(screen_base & 0b0111'1110'0000'0000) |
				((value << 1) & 0b0000'0001'1100'0000);
		break;
		case 0x07: {
			uint8_t mode = (value >> 3)&7;
			mode_40 = mode >= 4;
			mode_text = mode == 3 || mode == 6;

			switch(mode) {
				case 0:
				case 1:
				case 2:		mode_base = 0x3000;	break;
				case 3:		mode_base = 0x4000;	break;
				case 6:		mode_base = 0x6000;	break;
				default:	mode_base = 0x5800;	break;
			}

			switch(mode) {
				default:	mode_bpp = Bpp::One;	break;
				case 1:
				case 5:		mode_bpp = Bpp::Two;	break;
				case 2:		mode_bpp = Bpp::Four;	break;
			}
		} break;
		case 0x08: case 0x09: case 0x0a: case 0x0b:
		case 0x0c: case 0x0d: case 0x0e: case 0x0f: {
//			constexpr int registers[4][4] = {
//				{10, 8, 2, 0},
//				{14, 12, 6, 4},
//				{15, 13, 7, 5},
//				{11, 9, 3, 1},
//			};
//			const int index = (address >> 1)&3;
//			const uint8_t colour = ~value;
//			if(address&1) {
//				palette_[registers[index][0]]	= (palette_[registers[index][0]]&3)	| ((colour >> 1)&4);
//				palette_[registers[index][1]]	= (palette_[registers[index][1]]&3)	| ((colour >> 0)&4);
//				palette_[registers[index][2]]	= (palette_[registers[index][2]]&3)	| ((colour << 1)&4);
//				palette_[registers[index][3]]	= (palette_[registers[index][3]]&3)	| ((colour << 2)&4);
//
//				palette_[registers[index][2]]	= (palette_[registers[index][2]]&5)	| ((colour >> 4)&2);
//				palette_[registers[index][3]]	= (palette_[registers[index][3]]&5)	| ((colour >> 3)&2);
//			} else {
//				palette_[registers[index][0]]	= (palette_[registers[index][0]]&6)	| ((colour >> 7)&1);
//				palette_[registers[index][1]]	= (palette_[registers[index][1]]&6)	| ((colour >> 6)&1);
//				palette_[registers[index][2]]	= (palette_[registers[index][2]]&6)	| ((colour >> 5)&1);
//				palette_[registers[index][3]]	= (palette_[registers[index][3]]&6)	| ((colour >> 4)&1);
//
//				palette_[registers[index][0]]	= (palette_[registers[index][0]]&5)	| ((colour >> 2)&2);
//				palette_[registers[index][1]]	= (palette_[registers[index][1]]&5)	| ((colour >> 1)&2);
//			}
//
//			// regenerate all palette tables for now
//			for(int byte = 0; byte < 256; byte++) {
//				uint8_t *target = reinterpret_cast<uint8_t *>(&palette_tables_.forty1bpp[byte]);
//				target[0] = palette_[(byte&0x80) >> 4];
//				target[1] = palette_[(byte&0x40) >> 3];
//				target[2] = palette_[(byte&0x20) >> 2];
//				target[3] = palette_[(byte&0x10) >> 1];
//
//				target = reinterpret_cast<uint8_t *>(&palette_tables_.eighty2bpp[byte]);
//				target[0] = palette_[((byte&0x80) >> 4) | ((byte&0x08) >> 2)];
//				target[1] = palette_[((byte&0x40) >> 3) | ((byte&0x04) >> 1)];
//				target[2] = palette_[((byte&0x20) >> 2) | ((byte&0x02) >> 0)];
//				target[3] = palette_[((byte&0x10) >> 1) | ((byte&0x01) << 1)];
//
//				target = reinterpret_cast<uint8_t *>(&palette_tables_.eighty1bpp[byte]);
//				target[0] = palette_[(byte&0x80) >> 4];
//				target[1] = palette_[(byte&0x40) >> 3];
//				target[2] = palette_[(byte&0x20) >> 2];
//				target[3] = palette_[(byte&0x10) >> 1];
//				target[4] = palette_[(byte&0x08) >> 0];
//				target[5] = palette_[(byte&0x04) << 1];
//				target[6] = palette_[(byte&0x02) << 2];
//				target[7] = palette_[(byte&0x01) << 3];
//
//				target = reinterpret_cast<uint8_t *>(&palette_tables_.forty2bpp[byte]);
//				target[0] = palette_[((byte&0x80) >> 4) | ((byte&0x08) >> 2)];
//				target[1] = palette_[((byte&0x40) >> 3) | ((byte&0x04) >> 1)];
//
//				target = reinterpret_cast<uint8_t *>(&palette_tables_.eighty4bpp[byte]);
//				target[0] = palette_[((byte&0x80) >> 4) | ((byte&0x20) >> 3) | ((byte&0x08) >> 2) | ((byte&0x02) >> 1)];
//				target[1] = palette_[((byte&0x40) >> 3) | ((byte&0x10) >> 2) | ((byte&0x04) >> 1) | ((byte&0x01) >> 0)];
//			}
		} break;
	}
}

