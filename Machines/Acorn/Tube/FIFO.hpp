//
//  Header.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 30/10/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

#include <array>
#include <cstdint>

namespace Acorn::Tube {

template <size_t length, typename ULAT>
struct FIFO {
	FIFO(ULAT &ula, const uint8_t mask = 0x00) : ula_(ula), mask_(mask) {}

	/// @returns b7 set exactly if this FIFO is not empty.
	uint8_t data_available() const {
		return read_ != write_ ? 0x80 : 0x00;
	}

	/// @returns b6 set exactly if this FIFO is not full.
	uint8_t not_full() const {
		return size_t(write_ - read_) < length ? 0x40 : 0x00;
	}

	/// Adds a value to the FIFO, notifying the ULA if this would cause the FIFO to
	/// transition from empty to not-empty. The ULA will be supplied with this FIFO's
	/// mask as specified at construction.
	void write(const uint8_t value) {
		if(write_ - read_ == length) return;
		if(write_ == read_) {
			ula_.fifo_has_data(mask_);
		}
		buffer_[(write_++) % length] = value;
	}

	/// Removes a value from the FIFO.
	uint8_t read() {
		const uint8_t result = buffer_[read_ % length];
		if(write_ != read_) ++read_;
		return result;
	}

	/// Empties the FIFO.
	void reset() {
		read_ = write_ = 0;
	}

private:
	ULAT &ula_;
	uint8_t mask_;
	std::array<uint8_t, length> buffer_;
	uint32_t read_ = 0;
	uint32_t write_ = 0;
};

}
