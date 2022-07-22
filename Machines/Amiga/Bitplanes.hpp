//
//  Bitplanes.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 26/11/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef Bitplanes_hpp
#define Bitplanes_hpp

#include <cstdint>

#include "DMADevice.hpp"

namespace Amiga {

struct BitplaneData: public std::array<uint16_t, 6> {
	BitplaneData &operator <<= (int c) {
		(*this)[0] <<= c;
		(*this)[1] <<= c;
		(*this)[2] <<= c;
		(*this)[3] <<= c;
		(*this)[4] <<= c;
		(*this)[5] <<= c;
		return *this;
	}

	void clear() {
		std::fill(begin(), end(), 0);
	}
};

class Bitplanes: public DMADevice<6, 2> {
	public:
		using DMADevice::DMADevice;

		bool advance_dma(int cycle);
		void do_end_of_line();
		void set_control(uint16_t);

	private:
		bool is_high_res_ = false;
		int plane_count_ = 0;

		BitplaneData next;
};

template <typename SourceT>  constexpr SourceT bitplane_swizzle(SourceT value) {
	return
		(value&0x21) |
		((value&0x02) << 2) |
		((value&0x04) >> 1) |
		((value&0x08) << 1) |
		((value&0x10) >> 2);
}

class BitplaneShifter {
	public:
		/// Installs a new set of output pixels.
		void set(
			const BitplaneData &previous,
			const BitplaneData &next,
			int odd_delay,
			int even_delay);

		/// Shifts either two pixels (in low-res mode) or four pixels (in high-res).
		void shift(bool high_res) {
			constexpr int shifts[] = {16, 32};

			data_[1] = (data_[1] << shifts[high_res]) | (data_[0] >> (64 - shifts[high_res]));
			data_[0] <<= shifts[high_res];
		}

		/// @returns The next four pixels to output; in low-resolution mode only two
		/// of them will be unique.
		///
		/// The value is arranges so that MSB = first pixel to output, LSB = last.
		///
		/// Each byte is swizzled to provide easier playfield separation, being in the form:
		/// 	b6, b7 = 0;
		/// 	b3–b5: planes 1, 3 and 5;
		/// 	b0–b2: planes 0, 2 and 4.
		uint32_t get(bool high_res) {
			if(high_res) {
				return uint32_t(data_[1] >> 32);
			} else {
				uint32_t result = uint16_t(data_[1] >> 48);
				result = ((result & 0xff00) << 8) | (result & 0x00ff);
				result |= result << 8;
				return result;
			}
		}

	private:
		std::array<uint64_t, 2> data_{};

};

}

#endif /* Bitplanes_hpp */
