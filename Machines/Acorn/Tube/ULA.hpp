//
//  ULA.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/11/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

namespace Acorn::Tube {

/*!
	The non-FIFO section of the tube ULA.
*/
struct ULA {
	uint8_t status() const {
		return flags_;
	}

	void set_status(const uint8_t value) {
		const uint8_t bits = value & 0x3f;
		if(value & 0x80) {
			flags_ |= bits;
		} else {
			flags_ &= ~bits;
		}
	}

private:
	uint8_t flags_ = 0x3f;
};

}
