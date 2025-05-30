//
//  Sizes.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 26/01/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#pragma once

#include <cstdint>
#include <limits>
#include <type_traits>

template <int size> struct uint_t_impl;
template <> struct uint_t_impl<8> { using type = uint8_t; };
template <> struct uint_t_impl<16> { using type = uint16_t; };
template <> struct uint_t_impl<32> { using type = uint32_t; };
template <> struct uint_t_impl<64> { using type = uint64_t; };

/// Unsigned integer types templated on size; `uint_t<8> = uint8_t`; `uint_t<16> = uint16_t`, etc.
template <int size> using uint_t = typename uint_t_impl<size>::type;

/*!
	Maps to the smallest integral type that can contain `max_value`, from the following options:

	* uint8_t;
	* uint16_t;
	* uint32_t; or
	* uint64_t.
*/
template <uint64_t max_value>
struct MinIntForValue {
	using type =
		std::conditional_t<
			max_value <= std::numeric_limits<uint8_t>::max(), uint8_t,
			std::conditional_t<
				max_value <= std::numeric_limits<uint16_t>::max(), uint16_t,
				std::conditional_t<
					max_value <= std::numeric_limits<uint32_t>::max(), uint32_t,
					uint64_t
				>
			>
		>;
};
template <uint64_t max_value> using min_int_for_value_t = typename MinIntForValue<max_value>::type;

/*!
	Maps to the smallest integral type that can hold at least `max_bits` bits, from the following options:

	* uint8_t;
	* uint16_t;
	* uint32_t; or
	* uint64_t.
*/
template <int min_bits>
struct MinIntForBits {
	static_assert(min_bits <= 64, "Only integers up to 64 bits are supported");
	using type =
		std::conditional_t<
			min_bits <= 8, uint8_t,
			std::conditional_t<
				min_bits <= 16, uint16_t,
				std::conditional_t<
					min_bits <= 32, uint32_t,
					uint64_t
				>
			>
		>;
};
template <int min_bits> using min_int_for_bits_t = typename MinIntForBits<min_bits>::type;

static_assert(std::is_same_v<min_int_for_bits_t<7>, uint8_t>);
static_assert(std::is_same_v<min_int_for_bits_t<8>, uint8_t>);
static_assert(std::is_same_v<min_int_for_bits_t<9>, uint16_t>);
static_assert(std::is_same_v<min_int_for_bits_t<16>, uint16_t>);
static_assert(std::is_same_v<min_int_for_bits_t<17>, uint32_t>);
