//
//  CRC.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/09/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#ifndef CRC_hpp
#define CRC_hpp

#include <cstdint>
#include <vector>

namespace CRC {

/*! Provides a class capable of generating a CRC from source data. */
template <typename T, T reset_value, T xor_output, bool reflect_input, bool reflect_output> class Generator {
	public:
		/*!
			Instantiates a CRC16 that will compute the CRC16 specified by the supplied
			@c polynomial and @c reset_value.
		*/
		Generator(T polynomial): value_(reset_value) {
			const T top_bit = T(~(T(~0) >> 1));
			for(int c = 0; c < 256; c++) {
				T shift_value = static_cast<T>(c << multibyte_shift);
				for(int b = 0; b < 8; b++) {
					T exclusive_or = (shift_value&top_bit) ? polynomial : 0;
					shift_value = static_cast<T>(shift_value << 1) ^ exclusive_or;
				}
				xor_table[c] = shift_value;
			}
		}

		/// Resets the CRC to the reset value.
		void reset() { value_ = reset_value; }

		/// Updates the CRC to include @c byte.
		void add(uint8_t byte) {
			if(reflect_input) byte = reverse_byte(byte);
			value_ = static_cast<T>((value_ << 8) ^ xor_table[(value_ >> multibyte_shift) ^ byte]);
		}

		/// @returns The current value of the CRC.
		inline T get_value() const {
			T result = value_^xor_output;
			if(reflect_output) {
				T reflected_output = 0;
				for(std::size_t c = 0; c < sizeof(T); ++c) {
					reflected_output = T(reflected_output << 8) | T(reverse_byte(result & 0xff));
					result >>= 8;
				}
				return reflected_output;
			}
			return result;
		}

		/// Sets the current value of the CRC.
		inline void set_value(T value) { value_ = value; }

		/*!
			A compound for:

				reset()
				[add all data from @c data]
				get_value()
		*/
		T compute_crc(const std::vector<uint8_t> &data) {
			reset();
			for(const auto &byte: data) add(byte);
			return get_value();
		}

	private:
		static constexpr int multibyte_shift = (sizeof(T) * 8) - 8;
		T xor_table[256];
		T value_;

		constexpr uint8_t reverse_byte(uint8_t byte) const {
			return
				((byte & 0x80) ? 0x01 : 0x00) |
				((byte & 0x40) ? 0x02 : 0x00) |
				((byte & 0x20) ? 0x04 : 0x00) |
				((byte & 0x10) ? 0x08 : 0x00) |
				((byte & 0x08) ? 0x10 : 0x00) |
				((byte & 0x04) ? 0x20 : 0x00) |
				((byte & 0x02) ? 0x40 : 0x00) |
				((byte & 0x01) ? 0x80 : 0x00);
		}
};

/*!
	Provides a generator of 16-bit CCITT CRCs, which amongst other uses are
	those used by the FM and MFM disk encodings.
*/
struct CCITT: public Generator<uint16_t, 0xffff, 0x0000, false, false> {
	CCITT(): Generator(0x1021) {}
};

/*!
	Provides a generator of "standard 32-bit" CRCs.
*/
struct CRC32: public Generator<uint32_t, 0xffffffff, 0xffffffff, true, true> {
	CRC32(): Generator(0x04c11db7) {}
};

}

#endif /* CRC_hpp */
