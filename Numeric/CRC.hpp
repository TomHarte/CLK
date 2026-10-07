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
#include <concepts>
#include <cstdint>
#include <ranges>

namespace CRC {

/*! Provides a class capable of generating a CRC from source data. */
template <
	std::unsigned_integral IntT,
	IntT polynomial,
	IntT reset_value,
	IntT output_xor,
	bool reflect_input,
	bool reflect_output
>
class Generator {
public:
	constexpr Generator() noexcept: value_(reset_value) {}

	/// Resets the CRC to the reset value.
	void reset() { value_ = reset_value; }

	/// Updates the CRC to include @c byte.
	void add(uint8_t byte) {
		static constexpr std::array<IntT, 256> xor_table = [] {
			std::array<IntT, 256> table{};
			constexpr IntT top_bit = Numeric::top_bit<IntT>();
			for(size_t c = 0; c < 256; c++) {
				IntT shift_value = IntT(c << multibyte_shift);
				for(int b = 0; b < 8; b++) {
					const IntT exclusive_or = (shift_value & top_bit) ? polynomial : 0;
					shift_value = IntT(shift_value << 1) ^ exclusive_or;
				}
				table[c] = shift_value;
			}
			return table;
		} ();

		if constexpr (reflect_input) byte = Numeric::bit_reverse(byte);
		value_ = IntT((value_ << 8) ^ xor_table[(value_ >> multibyte_shift) ^ byte]);
	}

	/// @returns The current value of the CRC.
	IntT get_value() const {
		const IntT result = value_ ^ output_xor;
		if constexpr (reflect_output) {
			return Numeric::bit_reverse(result);
		} else {
			return result;
		}
	}

	/// Sets the current value of the CRC.
	void set_value(const IntT value) { value_ = value; }

	/*!
		Calculates the CRC of the provided range, assuming that it contains `uint8_t`s.
	*/
	template <std::ranges::range R>
	static IntT crc_of(const R &data) {
		Generator generator;
		for(const auto &byte : data) {
			generator.add(byte);
		}
		return generator.get_value();
	}

private:
	static constexpr int multibyte_shift = (sizeof(IntT) * 8) - 8;
	IntT value_;
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
