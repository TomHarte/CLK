//
//  Header.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 30/10/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

namespace Acorn::Tube {

template <size_t length>
struct FIFO {
	uint8_t status() const {
		return
			((read != write) ? 0x80 : 0x00) |
			((write - read < length) ? 0x40 : 0x00);
	}

	void write(const uint8_t value) {
		if(write - read == length) return;
		buffer[write++] = value;
	}

	void read() {
		const uint8_t result = buffer[read];
		if(write != read) ++read;
	}

private:
	std::array<uint8_t, length> buffer;
	uint32_t read = 0;
	uint32_t write = 0;
};

}
