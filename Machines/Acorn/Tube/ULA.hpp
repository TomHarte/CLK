//
//  ULA.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/11/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

namespace Acorn::Tube {

struct ULA {
	uint8_t flags() const {
		return flags_;
	}

	void set_flags(const uint8_t value) {
		const uint8_t bits = value & 0x7f;
		if(value & 0x80) {
			flags_ |= bits;
		} else {
			flags_ &= ~bits;
		}
	}

private:
	uint8_t flags_ = 0xff;
};

}
