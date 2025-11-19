//
//  SAA5050.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 24/09/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

#include <cstdint>
#include "Numeric/SizedInt.hpp"

namespace Mullard {

struct SAA5050Serialiser {
public:
	void begin_frame(bool is_odd);
	void begin_line();

	void add(Numeric::SizedInt<7>);

	struct Output {
		void reset() {
			top_ = bottom_ = 0;
		}
		void load(const uint8_t top, const uint8_t bottom) {
			top_ = top;
			bottom_ = bottom;
		}
		void load(const uint8_t top) {
			top_ = bottom_ = top;
		}

		// The low twelve bits of this word provide 1bpp pixels.
		uint16_t pixels() const {
			// Adapted from old ElectrEm source; my original provenance for this being the correct logic is unknown.
			uint16_t wide =
				((top_ & 0b000001) ? 0b0000'0000'0011 : 0) |
				((top_ & 0b000010) ? 0b0000'0000'1100 : 0) |
				((top_ & 0b000100) ? 0b0000'0011'0000 : 0) |
				((top_ & 0b001000) ? 0b0000'1100'0000 : 0) |
				((top_ & 0b010000) ? 0b0011'0000'0000 : 0) |
				((top_ & 0b100000) ? 0b1100'0000'0000 : 0);

			if(top_ != bottom_) {
				if((top_ & 0b10000) && (bottom_ & 0b11000) == 0b01000) wide |= 0b0000'1000'0000;
				if((top_ & 0b01000) && (bottom_ & 0b01100) == 0b00100) wide |= 0b0000'0010'0000;
				if((top_ & 0b00100) && (bottom_ & 0b00110) == 0b00010) wide |= 0b0000'0000'1000;
				if((top_ & 0b00010) && (bottom_ & 0b00011) == 0b00001) wide |= 0b0000'0000'0010;

				if((top_ & 0b01000) && (bottom_ & 0b11000) == 0b10000) wide |= 0b0001'0000'0000;
				if((top_ & 0b00100) && (bottom_ & 0b01100) == 0b01000) wide |= 0b0000'0100'0000;
				if((top_ & 0b00010) && (bottom_ & 0b00110) == 0b00100) wide |= 0b0000'0001'0000;
				if((top_ & 0b00001) && (bottom_ & 0b00011) == 0b00010) wide |= 0b0000'0000'0100;
			}

			return wide;
		}

		// Colours for foreground and background pixels.
		uint8_t alpha;
		uint8_t background;

	private:
		uint8_t top_, bottom_;
	};
	bool has_output() const;
	Output output();

	void set_reveal(bool);

private:
	Output output_;
	bool has_output_ = false;

	int row_, line_;
	bool odd_frame_;

	bool flash_ = false;
	int frame_counter_ = 0;

	bool reveal_ = false;
	bool conceal_ = false;

	bool alpha_mode_ = true;
	bool separated_graphics_ = false;

	bool double_height_ = false;
	bool row_has_double_height_ = false;
	int double_height_offset_ = 0;

	bool hold_graphics_ = false;
	uint8_t last_graphic_ = 0;

	void load_pixels(const uint8_t);
	void apply_control(const uint8_t);
};

}
