//
//  9918.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/11/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "9918.hpp"

using namespace TI;

namespace {

const uint32_t palette_pack(uint8_t r, uint8_t g, uint8_t b) {
	uint32_t result = 0;
	uint8_t *result_ptr = reinterpret_cast<uint8_t *>(&result);
	result_ptr[0] = r;
	result_ptr[1] = g;
	result_ptr[2] = b;
	result_ptr[3] = 0;
	return result;
}

const uint32_t palette[16] = {
	palette_pack(0, 0, 0),
	palette_pack(0, 0, 0),
	palette_pack(90, 201, 81),
	palette_pack(149, 231, 133),

	palette_pack(113, 104, 183),
	palette_pack(146, 132, 255),
	palette_pack(200, 114, 89),
	palette_pack(115, 222, 255),

	palette_pack(238, 124, 90),
	palette_pack(255, 166, 132),
	palette_pack(219, 232, 92),
	palette_pack(240, 247, 143),

	palette_pack(78, 176, 63),
	palette_pack(202, 118, 216),
	palette_pack(233, 233, 233),
	palette_pack(255, 255, 255)
};

}

TMS9918::TMS9918(Personality p) :
	crt_(new Outputs::CRT::CRT(342, 1, Outputs::CRT::DisplayType::NTSC60, 4)) {
	crt_->set_rgb_sampling_function(
		"vec3 rgb_sample(usampler2D sampler, vec2 coordinate, vec2 icoordinate)"
		"{"
			"return texture(sampler, coordinate).rgb / vec3(255.0);"
		"}");
	crt_->set_output_device(Outputs::CRT::OutputDevice::Monitor);
}

std::shared_ptr<Outputs::CRT::CRT> TMS9918::get_crt() {
	return crt_;
}

void TMS9918::run_for(const HalfCycles cycles) {
	// As specific as I've been able to get:
	// Scanline time is always 227.75 cycles.
	// PAL output is 313 lines total. NTSC output is 262 lines total.
	// Interrupt is signalled upon entering the lower border.

	// Keep a count of cycles separate from internal counts to avoid
	// potential errors mapping back and forth.
	half_cycles_into_frame_ = (half_cycles_into_frame_ + cycles) % HalfCycles(frame_lines_ * 228 * 2);

	// Convert to 342 cycles per line; the internal clock is 1.5 times the
	// nominal 3.579545 Mhz that I've advertised for this part.
	int int_cycles = (cycles.as_int() * 3) + cycles_error_;
	cycles_error_ = int_cycles & 7;
	int_cycles >>= 3;
	if(!int_cycles) return;

	//
	// Break that down as:
	// 26 cycles sync;

	while(int_cycles) {
		int cycles_left = std::min(342 - column_, int_cycles);
		column_ += cycles_left;

		if(row_	< 192) {
			// Do memory accesses.
			switch(next_screen_mode_) {
				default:	// TODO: other graphics mode; this is graphics 1.
					while(access_pointer_ < (column_ >> 1)) {
						if(access_pointer_ < 26) {
							access_pointer_ = std::min(26, column_ >> 1);
						}
						if(access_pointer_ >= 26) {
							int end = std::min(154, column_);

							// TODO: optimise this mess.
							const int row_base = ((row_ << 2)&~31);
							while(access_pointer_ < end) {
								int character_column = ((access_pointer_ - 26) >> 2);
								switch(access_pointer_&3) {
									case 0:
										pattern_name_ = ram_[pattern_name_address_ + row_base + character_column];
									break;
									case 1:	break;	// TODO: sprites / CPU access.
									case 2:
										colour_buffer_[character_column] = ram_[colour_table_address_ + (pattern_name_ >> 3)];
									break;
									case 3:
										pattern_buffer_[character_column] = ram_[pattern_generator_table_address_ + (pattern_name_ << 3) + (row_ & 7)];
									break;
								}
								access_pointer_++;
							}
						}
						if(access_pointer_ >= 154) {
							access_pointer_ = column_ >> 1;
						}
					}
				break;
			}

			// Pixels.
			if(!output_column_ && column_ >= 26) {
				crt_->output_sync(static_cast<unsigned int>(26));
				output_column_ = 26;
			}
			// TODO: colour burst.
			if(output_column_ >= 26) {
				int pixels_end = std::min(first_pixel_column_, column_);
				if(output_column_ < pixels_end) {
					output_border(pixels_end - output_column_);
					output_column_ = pixels_end;

					if(pixels_end == first_pixel_column_) {
						pixel_target_ = reinterpret_cast<uint32_t *>(crt_->allocate_write_area(static_cast<unsigned int>(first_right_border_column_ - first_pixel_column_)));
					}
				}
			}
			if(output_column_ >= first_pixel_column_) {
				int pixels_end = std::min(first_right_border_column_, column_);
				if(output_column_ < pixels_end) {
					while(output_column_ < pixels_end) {
						int base = (output_column_ - first_pixel_column_);
						int address = base >> 3;
						uint8_t colour = colour_buffer_[address];
						uint8_t pattern = pattern_buffer_[address];
						pattern >>= ((base&7)^7);

						*pixel_target_ = (pattern&1) ? palette[colour >> 4] : palette[colour & 15];
						pixel_target_ ++;
						output_column_ ++;
					}

					if(output_column_ == first_right_border_column_) {
						crt_->output_data(static_cast<unsigned int>(first_right_border_column_ - first_pixel_column_), 1);
					}
				}
			}
			if(column_ >= first_pixel_column_) {
				output_border(column_ - output_column_);
				output_column_ = column_;
			}
		} else if(row_ >= first_vsync_line_ && row_ < first_vsync_line_+3) {
			// Vertical sync.
			if(column_ == 342) {
				crt_->output_sync(static_cast<unsigned int>(342));
			}
		} else {
			// Blank.
			if(!output_column_ && column_ >= 26) {
				crt_->output_sync(static_cast<unsigned int>(26));
				output_column_ = 26;
			}
			if(column_ >= 26) {
				output_border(column_ - output_column_);
				output_column_ = column_;
			}
		}

		int_cycles -= cycles_left;
		if(column_ == 342) {
			access_pointer_ = column_ = output_column_ = 0;
			row_ = (row_ + 1) % frame_lines_;

			// TODO: consider triggering an interrupt here.
			screen_mode_ = next_screen_mode_;
			switch(screen_mode_) {
				// TODO: text mdoe.
				default:
					line_mode_ = LineMode::Character;
					first_pixel_column_ = 63;
					first_right_border_column_ = 319;
				break;
			}
		}
	}
}

void TMS9918::output_border(int cycles) {
	pixel_target_ = reinterpret_cast<uint32_t *>(crt_->allocate_write_area(1));
	if(pixel_target_) *pixel_target_ = palette[background_colour_];
	crt_->output_level(static_cast<unsigned int>(cycles));
}

// TODO: as a temporary development measure, memory access below is magically instantaneous. Correct that.

void TMS9918::set_register(int address, uint8_t value) {
	// Writes to address 0 are writes to the video RAM. Store
	// the value and return.
	if(!(address & 1)) {
		write_phase_ = false;
		read_ahead_buffer_ = value;
		ram_[ram_pointer_ & 16383] = value;
		ram_pointer_++;
		return;
	}

	// Writes to address 1 are performed in pairs; if this is the
	// low byte of a value, store it and wait for the high byte.
	if(!write_phase_) {
		low_write_ = value;
		write_phase_ = true;
		return;
	}

	write_phase_ = false;
	if(value & 0x80) {
		// This is a write to a register.
		switch(value & 7) {
			case 0:
				next_screen_mode_ = (next_screen_mode_ & 6) | ((low_write_ & 2) >> 1);
			break;

			case 1:
				blank_screen_ = !!(low_write_ & 0x40);
				generate_interrupts_ = !!(low_write_ & 0x20);
				next_screen_mode_ = (screen_mode_ & 1) | ((low_write_ & 0x18) >> 3);
				sprites_16x16_ = !!(low_write_ & 0x02);
				sprites_magnified_ = !!(low_write_ & 0x01);
				reevaluate_interrupts();
			break;

			case 2:
				pattern_name_address_ = static_cast<uint16_t>((low_write_ & 0xf) << 10);
			break;

			case 3:
				colour_table_address_ = static_cast<uint16_t>(low_write_ << 6);
			break;

			case 4:
				pattern_generator_table_address_ = static_cast<uint16_t>((low_write_ & 0x07) << 11);
			break;

			case 5:
				sprite_attribute_table_address_ = static_cast<uint16_t>((low_write_ & 0x7f) << 7);
			break;

			case 6:
				sprite_generator_table_address_ = static_cast<uint16_t>((low_write_ & 0x07) << 11);
			break;

			case 7:
				text_colour_ = low_write_ >> 4;
				background_colour_ = low_write_ & 0xf;
			break;
		}
	} else {
		// This is a write to the RAM pointer.
		ram_pointer_ = static_cast<uint16_t>(low_write_ | (value << 8));
		if(!(value & 0x40)) {
			// Officially a 'read' set, so perform lookahead.
			read_ahead_buffer_ = ram_[ram_pointer_ & 16383];
			ram_pointer_++;
		}
	}
}

uint8_t TMS9918::get_register(int address) {
	write_phase_ = false;

	// Reads from address 0 read video RAM, via the read-ahead buffer.
	if(!(address & 1)) {
		uint8_t result = read_ahead_buffer_;
		read_ahead_buffer_ = ram_[ram_pointer_ & 16383];
		ram_pointer_++;
		return result;
	}

	// Reads from address 1 get the status register;
	uint8_t result = status_;
	status_ &= ~(0x80 | 0x20);
	reevaluate_interrupts();
	return result;
}

void TMS9918::reevaluate_interrupts() {
	
}

HalfCycles TMS9918::get_time_until_interrupt() {
	if(!generate_interrupts_) return HalfCycles(-1);
	if(get_interrupt_line()) return HalfCycles(-1);

	const int half_cycles_per_frame = frame_lines_ * 228 * 2;
	int half_cycles_remaining = (192 * 228 * 2 + half_cycles_per_frame - half_cycles_into_frame_.as_int()) % half_cycles_per_frame;
	return HalfCycles(half_cycles_remaining);
}

bool TMS9918::get_interrupt_line() {
	return (status_ & 0x80) && generate_interrupts_;
}
