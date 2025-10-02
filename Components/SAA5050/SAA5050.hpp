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
		// The low twelve bits of this word provide 1bpp pixels.
		uint16_t pixels;

		// Colours for set and background pixels.
		uint8_t alpha;
		uint8_t background;
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

	uint16_t pixels(const uint8_t);
	void apply_control(const uint8_t);
};

}
