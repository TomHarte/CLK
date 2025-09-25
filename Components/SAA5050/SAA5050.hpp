//
//  SAA5050.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 24/09/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

#include <cstdint>

namespace Mullard {

struct SAA5050Serialiser {
public:
	void begin_frame(bool is_odd);
	void begin_line();

	void add(uint8_t);

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
	int row_, line_;
	Output output_;
	bool has_output_ = false;
	bool odd_frame_;

	// TODO: more state. Graphics mode only, probably?
};

}
