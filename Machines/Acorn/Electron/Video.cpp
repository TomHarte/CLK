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

// MARK: - Lifecycle

VideoOutput::VideoOutput(const uint8_t *memory) :
	ram_(memory),
	crt_(h_total,
		1,
		Outputs::Display::Type::PAL50,
		Outputs::Display::InputDataType::Red1Green1Blue1) {
	crt_.set_visible_area(crt_.get_rect_for_area(
		312 - vsync_end,
		256,
		h_total - hsync_start,
		80 * 8,
		4.0f / 3.0f
	));
}

void VideoOutput::set_scan_target(Outputs::Display::ScanTarget *scan_target) {
	crt_.set_scan_target(scan_target);
}

Outputs::Display::ScanStatus VideoOutput::get_scaled_scan_status() const {
	return crt_.get_scaled_scan_status();
}

void VideoOutput::set_display_type(Outputs::Display::DisplayType display_type) {
	crt_.set_display_type(display_type);
}

Outputs::Display::DisplayType VideoOutput::get_display_type() const {
	return crt_.get_display_type();
}

uint8_t VideoOutput::run_for(const Cycles cycles) {
	uint8_t interrupts{};

	int number_of_cycles = cycles.as<int>();
	while(number_of_cycles--) {
		// The below is my attempt at transcription of the equivalent VHDL code in moogway82's
		// JamSoftElectronULA — https://github.com/moogway82/JamSoftElectronULA — which is itself
		// derived from hoglet67's https://github.com/hoglet67/ElectronFpga and that author's
		// reverse-engineering of the Electron ULA. It should therefore be as accurate to the
		// original hardware as my comprehension of VHDL and adaptation into sequential code allows.

		// In this, the sequential world of C++, all tests below should assume that the position
		// named by (h_count_, v_count_) is the one that was active **prior to this cycle**.
		//
		// So this cycle spans the period from (h_count_, v_count_) to (h_count_, v_count_)+1.

		// Test for interrupts.
		if(v_count_ == v_rtc && ((!field_ && !h_count_) || (field_ && h_count_ == h_half))) {
			interrupts |= static_cast<uint8_t>(Interrupt::RealTimeClock);
		}
		if(h_count_ == hsync_start && ((v_count_ == v_disp_gph && !mode_text_) or (v_count_ == v_disp_txt && mode_text_))) {
			interrupts |= static_cast<uint8_t>(Interrupt::DisplayEnd);
		}

		// Update syncs.
		if(!field_) {
			if(!h_count_ && v_count_ == vsync_start) {
				vsync_int_ = true;
			} else if(h_count_ == h_half && v_count_ == vsync_end) {
				vsync_int_ = false;
			}
		} else {
			if(h_count_ == h_half && v_count_ == vsync_start) {
				vsync_int_ = true;
			} else if(!h_count_ && v_count_ == vsync_end + 1) {
				vsync_int_ = false;
			}
		}
		
		if(h_count_ == hsync_start) {
			hsync_int_ = true;
		} else if(h_count_ == hsync_end) {
			hsync_int_ = false;
		}
		
		// Update character row on the trailing edge of hsync.
		if(h_count_ == hsync_end) {
			if(is_v_end()) {
				char_row_ = 0;
			} else {
				char_row_ = last_line() ? 0 : char_row_ + 1;
			}
		}

		// Disable the top bit of the char_row counter outside of text mode.
		if(!mode_text_) {
			char_row_ &= 7;
		}

		// Latch video address at frame start.
		if(h_count_ == h_reset_addr && is_v_end()) {
			row_addr_ = byte_addr_ = screen_base_;
		}
		
		// Copy byte_addr back into row_addr if a new character row has begun.
		if(hsync_int_) {
			if(last_line()) {
				row_addr_ = byte_addr_;
			} else {
				byte_addr_ = row_addr_;
			}
		}

		// Determine current output item.
		OutputStage stage;
		int screen_pitch = screen_pitch_;
		if(vsync_int_ || hsync_int_) {
			stage = OutputStage::Sync;
		} else if(in_blank()) {
			if(h_count_ >= hburst_start && h_count_ < hburst_end) {
				stage = OutputStage::ColourBurst;
			} else {
				stage = OutputStage::Blank;
			}
		} else {
			stage = OutputStage::Pixels;
			screen_pitch = (mode_40_ ? 320 : 640) / static_cast<int>(mode_bpp_);
		}

		if(stage != output_ || screen_pitch != screen_pitch_) {
			switch(output_) {
				case OutputStage::Sync:			crt_.output_sync(output_length_);					break;
				case OutputStage::Blank:		crt_.output_blank(output_length_);					break;
				case OutputStage::ColourBurst:	crt_.output_default_colour_burst(output_length_);	break;
				case OutputStage::Pixels:
					if(current_output_target_) {
						crt_.output_data(
							output_length_,
							static_cast<size_t>(current_output_target_ - initial_output_target_)
						);
					} else {
						crt_.output_data(output_length_);
					}
				break;
			}
			output_length_ = 0;
			output_ = stage;
			screen_pitch_ = screen_pitch;

			if(stage == OutputStage::Pixels) {
				initial_output_target_ = current_output_target_ = crt_.begin_data(static_cast<size_t>(screen_pitch_));
			}
		}
		output_length_ += 8;
		if(output_ == OutputStage::Pixels && (!mode_40_ || h_count_ & 8) && current_output_target_) {
			const uint8_t data = ram_[byte_addr_ | char_row_];

			switch(mode_bpp_) {
				case Bpp::One:
					current_output_target_[0] = palette1bpp_[(data >> 7) & 1];
					current_output_target_[1] = palette1bpp_[(data >> 6) & 1];
					current_output_target_[2] = palette1bpp_[(data >> 5) & 1];
					current_output_target_[3] = palette1bpp_[(data >> 4) & 1];
					current_output_target_[4] = palette1bpp_[(data >> 3) & 1];
					current_output_target_[5] = palette1bpp_[(data >> 2) & 1];
					current_output_target_[6] = palette1bpp_[(data >> 1) & 1];
					current_output_target_[7] = palette1bpp_[(data >> 0) & 1];
					current_output_target_ += 8;
				break;
				case Bpp::Two:
					current_output_target_[0] = palette2bpp_[((data >> 6) & 2) | ((data >> 3) & 1)];
					current_output_target_[1] = palette2bpp_[((data >> 5) & 2) | ((data >> 2) & 1)];
					current_output_target_[2] = palette2bpp_[((data >> 4) & 2) | ((data >> 1) & 1)];
					current_output_target_[3] = palette2bpp_[((data >> 3) & 2) | ((data >> 0) & 1)];
					current_output_target_ += 4;
				break;
				case Bpp::Four:
					current_output_target_[0] = palette4bpp_[((data >> 4) & 8) | ((data >> 3) & 4) | ((data >> 2) & 2) | ((data >> 1) & 1)];
					current_output_target_[1] = palette4bpp_[((data >> 3) & 8) | ((data >> 2) & 4) | ((data >> 1) & 2) | ((data >> 0) & 1)];
					current_output_target_ += 2;
				break;
			}
		}

		// Increment the byte address across the line.
		// (slghtly pained logic here because the input clock is still at the pixel rate, not the byte rate)
		if(h_count_ < h_active) {
			if(
				(!mode_40_ && !(h_count_ & 0x7)) ||
				(mode_40_ && ((h_count_ & 0xf) == 0x8))
			) {
				byte_addr_ += 8;

				if(!(byte_addr_ & 0b0111'1000'0000'0000)) {
					byte_addr_ = mode_base_ | (byte_addr_ & 0x0000'0111'1111'1111);
				}
			}
		}

		// Horizontal and vertical counter updates; code below should act
		h_count_ += 8;
		if(h_count_ == h_total) {
			h_count_ = 0;

			if(is_v_end()) {
				v_count_ = 0;
				field_ = !field_;
			} else {
				++v_count_;
			}
		}
	}

	return interrupts;
}

// MARK: - Register hub

void VideoOutput::write(int address, const uint8_t value) {
	address &= 0xf;
	switch(address) {
		case 0x02:
			screen_base_ =
				(screen_base_ & 0b0111'1110'0000'0000) |
				((value << 1) & 0b0000'0001'1100'0000);
		break;
		case 0x03:
			screen_base_ =
				((value << 9) & 0b0111'1110'0000'0000) |
				(screen_base_ & 0b0000'0001'1100'0000);
		break;
		case 0x07: {
			const uint8_t mode = (value >> 3)&7;
			mode_40_ = mode >= 4;
			mode_text_ = mode == 3 || mode == 6;

			switch(mode) {
				case 0:
				case 1:
				case 2:		mode_base_ = 0x3000;	break;
				case 3:		mode_base_ = 0x4000;	break;
				case 6:		mode_base_ = 0x6000;	break;
				default:	mode_base_ = 0x5800;	break;
			}

			switch(mode) {
				default:	mode_bpp_ = Bpp::One;	break;
				case 1:
				case 5:		mode_bpp_ = Bpp::Two;	break;
				case 2:		mode_bpp_ = Bpp::Four;	break;
			}
		} break;
		case 0x08: case 0x09: case 0x0a: case 0x0b:
		case 0x0c: case 0x0d: case 0x0e: case 0x0f: {
			palette_[address - 8] = ~value;

			if(address <= 0x09) {
				palette1bpp_[0] = palette_entry<{0xfe09, 0}, {0xfe09, 4}, {0xfe08, 4}>();
				palette1bpp_[1] = palette_entry<{0xfe09, 2}, {0xfe08, 6}, {0xfe08, 2}>();

				palette2bpp_[0] = palette_entry<{0xfe09, 0}, {0xfe09, 4}, {0xfe08, 4}>();
				palette2bpp_[1] = palette_entry<{0xfe09, 1}, {0xfe09, 5}, {0xfe08, 5}>();
				palette2bpp_[2] = palette_entry<{0xfe09, 2}, {0xfe08, 2}, {0xfe08, 6}>();
				palette2bpp_[3] = palette_entry<{0xfe09, 3}, {0xfe08, 3}, {0xfe08, 7}>();
			}

			palette4bpp_[0] = palette_entry<{0xfe09, 0}, {0xfe09, 4}, {0xfe08, 4}>();
			palette4bpp_[2] = palette_entry<{0xfe09, 1}, {0xfe09, 5}, {0xfe08, 5}>();
			palette4bpp_[8] = palette_entry<{0xfe09, 2}, {0xfe08, 2}, {0xfe08, 6}>();
			palette4bpp_[10] = palette_entry<{0xfe09, 3}, {0xfe08, 3}, {0xfe08, 7}>();

			palette4bpp_[4] = palette_entry<{0xfe0b, 0}, {0xfe0b, 4}, {0xfe0a, 4}>();
			palette4bpp_[6] = palette_entry<{0xfe0b, 1}, {0xfe0b, 5}, {0xfe0a, 5}>();
			palette4bpp_[12] = palette_entry<{0xfe0b, 2}, {0xfe0a, 2}, {0xfe0a, 6}>();
			palette4bpp_[14] = palette_entry<{0xfe0b, 3}, {0xfe0a, 3}, {0xfe0a, 7}>();

			palette4bpp_[5] = palette_entry<{0xfe0d, 0}, {0xfe0d, 4}, {0xfe0c, 4}>();
			palette4bpp_[7] = palette_entry<{0xfe0d, 1}, {0xfe0d, 5}, {0xfe0c, 5}>();
			palette4bpp_[13] = palette_entry<{0xfe0d, 2}, {0xfe0c, 2}, {0xfe0c, 6}>();
			palette4bpp_[15] = palette_entry<{0xfe0d, 3}, {0xfe0c, 3}, {0xfe0c, 7}>();

			palette4bpp_[1] = palette_entry<{0xfe0f, 0}, {0xfe0f, 4}, {0xfe0e, 4}>();
			palette4bpp_[3] = palette_entry<{0xfe0f, 1}, {0xfe0f, 5}, {0xfe0e, 5}>();
			palette4bpp_[9] = palette_entry<{0xfe0f, 2}, {0xfe0e, 2}, {0xfe0e, 6}>();
			palette4bpp_[11] = palette_entry<{0xfe0f, 3}, {0xfe0e, 3}, {0xfe0e, 7}>();
		} break;
	}
}
