//
//  9918.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/11/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "9918.hpp"

using namespace TI;

TMS9918::TMS9918(Personality p) :
	crt_(new Outputs::CRT::CRT(342, 1, Outputs::CRT::DisplayType::NTSC60, 4)) {
	crt_->set_rgb_sampling_function(
		"vec3 rgb_sample(usampler2D sampler, vec2 coordinate, vec2 icoordinate)"
		"{"
			"return texture(sampler, coordinate).rgb;"
		"}");
}

std::shared_ptr<Outputs::CRT::CRT> TMS9918::get_crt() {
	return crt_;
}

void TMS9918::run_for(const HalfCycles cycles) {
	// TODO: all video output (!)

	// As specific as I've been able to get:
	// Scanline time is always 227.75 cycles.
	// PAL output is 313 lines total. NTSC output is 262 lines total.
	// Interrupt is signalled upon entering the lower border.

	// Convert to 342 cycles per line; the internal clock is 1.5 times the
	// nominal 3.579545 Mhz that I've advertised for this part.
	int int_cycles = (cycles.as_int() * 3) + cycles_error_;
	cycles_error_ = int_cycles & 3;
	int_cycles >>= 2;

	//
	// Break that down as:
	// 26 cycles sync;

	while(int_cycles) {
		int cycles_left = std::min(342 - column_, int_cycles);
		int end_column = std::min(column_ + int_cycles, 342);
		if(row_	< 192) {
			// Pixels.
			if(column_ < 26 && end_column >= 26) {
				crt_->output_sync(static_cast<unsigned int>(26));
				column_ = 26;
			}
			if(column_ >= 26 && end_column >= 69) {	// TODO: modes other than text
				crt_->output_blank(static_cast<unsigned int>(69 - 26));
				column_ = 69;
				pixel_target_ = crt_->allocate_write_area(256);
			}
			while(column_ < end_column && column_ < 309) {	// TODO: modes other than text
				pixel_target_[0] = pixel_target_[1] = pixel_target_[2] = pixel_target_[3] = 0xff;
				pixel_target_ += 4;
				column_ ++;
			}
			if(column_ == 309 && end_column > 309) {
				crt_->output_data(240, 1);	// TODO: modes other than text
			}
			if(end_column == 342) {
				crt_->output_blank(static_cast<unsigned int>(342 - 309));
			}
		} else if(row_ >= 227 && row_ < 230) {	// TODO: don't hard-code NTSC.
			// Vertical sync.
			crt_->output_sync(static_cast<unsigned int>(cycles_left));
		} else {
			// Blank.
			if(column_ < 26 && end_column >= 26) {
				crt_->output_sync(static_cast<unsigned int>(26));
				column_ = 26;
			}
			if(column_ < end_column) {
				crt_->output_blank(static_cast<unsigned int>(end_column - column_));
				column_ = end_column;
			}
		}

		int_cycles -= cycles_left;
		column_ = end_column;
		if(column_ == 342) {
			column_ = 0;
			row_ = (row_ + 1) % 262;	// TODO: don't hard-code NTSC.
			// TODO: consider triggering an interrupt here.
		}
	}
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
