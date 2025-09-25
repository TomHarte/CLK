//
//  SAA5050.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 24/09/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

#include <cstdint>
#include "Numeric/SizedCounter.hpp"

namespace Mullard {

struct SAA5050Serialiser {
public:
	void begin_frame(bool is_odd);
	void begin_line();

	void add(Numeric::SizedCounter<7>);

	struct Output {
		// The low twelve bits of this word provide 1bpp pixels.
		uint16_t pixels;

		// Colours for set and background pixels.
		uint8_t alpha;
		uint8_t background;
	};
	bool has_output() const;
	Output output();

private:
	uint8_t next_control_ = 0;
	Output output_;
	bool has_output_ = false;

	int row_, line_;
	bool odd_frame_;

	bool alpha_mode_ = true;
	bool separated_graphics_ = false;
};

}
