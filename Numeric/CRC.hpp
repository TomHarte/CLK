//
//  CRC.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/09/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#pragma once

#include "BitReverse.hpp"
#include "Carry.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace CRC {

/*! Provides a class capable of generating a CRC from source data. */
template <
	typename IntType,
	IntType polynomial,
	IntType reset_value,
	IntType output_xor,
	bool reflect_input,
	bool reflect_output
>
class Generator {
public:
	/*!
		Instantiates a CRC16 that will compute the CRC16 specified by the supplied
		@c polynomial and @c reset_value.
	*/
	constexpr Generator() noexcept: value_(reset_value) {}

	/// Resets the CRC to the reset value.
	void reset() { value_ = reset_value; }

	/// Updates the CRC to include @c byte.
	void add(uint8_t byte) {
		static constexpr std::array<IntType, 256> xor_table = [] {
			std::array<IntType, 256> table{};
			constexpr IntType top_bit = Numeric::top_bit<IntType>();
			for(size_t c = 0; c < 256; c++) {
				IntType shift_value = IntType(c << multibyte_shift);
				for(int b = 0; b < 8; b++) {
					const IntType exclusive_or = (shift_value & top_bit) ? polynomial : 0;
					shift_value = IntType(shift_value << 1) ^ exclusive_or;
				}
				table[c] = shift_value;
			}
			return table;
		} ();

		if constexpr (reflect_input) byte = Numeric::bit_reverse(byte);
		value_ = IntType((value_ << 8) ^ xor_table[(value_ >> multibyte_shift) ^ byte]);
	}

	/// @returns The current value of the CRC.
	inline IntType get_value() const {
		const IntType result = value_ ^ output_xor;
		if constexpr (reflect_output) {
			return Numeric::bit_reverse(result);
		} else {
			return result;
		}
	}

	/// Sets the current value of the CRC.
	inline void set_value(const IntType value) { value_ = value; }

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
	template <typename Iterator> IntType compute_crc(Iterator begin, const Iterator end) {
		reset();
		while(begin != end) {
			add(*begin);
			++begin;
		}
		return get_value();
	}

private:
	static constexpr int multibyte_shift = (sizeof(IntType) * 8) - 8;
	IntType value_;
};

/*!
	Provides a generator of 16-bit CCITT CRCs, which amongst other uses are
	those used by the FM and MFM disk encodings.
*/
using CCITT = Generator<uint16_t, 0x1021, 0xffff, 0x0000, false, false>;

/*!
	Provides a generator of "standard 32-bit" CRCs.
*/
using CRC32 = Generator<uint32_t, 0x04c11db7, 0xffffffff, 0xffffffff, true, true>;

}
