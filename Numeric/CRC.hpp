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
template <typename IntType, IntType reset_value, IntType output_xor, bool reflect_input, bool reflect_output> class Generator {
	public:
		/*!
			Instantiates a CRC16 that will compute the CRC16 specified by the supplied
			@c polynomial and @c reset_value.
		*/
		Generator(IntType polynomial): value_(reset_value) {
			const IntType top_bit = IntType(~(IntType(~0) >> 1));
			for(int c = 0; c < 256; c++) {
				IntType shift_value = IntType(c << multibyte_shift);
				for(int b = 0; b < 8; b++) {
					IntType exclusive_or = (shift_value&top_bit) ? polynomial : 0;
					shift_value = IntType(shift_value << 1) ^ exclusive_or;
				}
				xor_table[c] = shift_value;
			}
		}

		/// Resets the CRC to the reset value.
		void reset() { value_ = reset_value; }

		/// Updates the CRC to include @c byte.
		void add(uint8_t byte) {
			if constexpr (reflect_input) byte = reverse_byte(byte);
			value_ = IntType((value_ << 8) ^ xor_table[(value_ >> multibyte_shift) ^ byte]);
		}

		/// @returns The current value of the CRC.
		inline IntType get_value() const {
			IntType result = value_ ^ output_xor;
			if constexpr (reflect_output) {
				IntType reflected_output = 0;
				for(std::size_t c = 0; c < sizeof(IntType); ++c) {
					reflected_output = IntType(reflected_output << 8) | IntType(reverse_byte(result & 0xff));
					result >>= 8;
				}
				return reflected_output;
			}
			return result;
		}

		/// Sets the current value of the CRC.
		inline void set_value(IntType value) { value_ = value; }

		/*!
			A compound for:

				reset()
				[add all data from @c data]
				get_value()
		*/
		template <typename Collection> IntType compute_crc(const Collection &data) {
			return compute_crc(data.begin(), data.end());
		}

		/*!
			A compound for:

				reset()
				[add all data from @c begin to @c end]
				get_value()
		*/
		template <typename Iterator> IntType compute_crc(Iterator begin, Iterator end) {
			reset();
			while(begin != end) {
				add(*begin);
				++begin;
			}
			return get_value();
		}

	private:
		static constexpr int multibyte_shift = (sizeof(IntType) * 8) - 8;
		IntType xor_table[256];
		IntType value_;

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
