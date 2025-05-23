//
//  Carry.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 30/08/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#pragma once

#include <limits>
#include <type_traits>

namespace Numeric {

enum class Operation {
	Add,
	Subtract,
};

template <int bit, typename IntT>
concept is_carry_source = (bit <= sizeof(IntT) * 8) && std::is_integral_v<IntT> && std::is_unsigned_v<IntT>;

/// @returns @c true if from @c bit there was:
///		• carry after calculating @c lhs + @c rhs if @c is_add is true; or
///		• borrow after calculating @c lhs - @c rhs if @c is_add is false;
/// producing @c result.
template <Operation operation, int bit, typename IntT>
requires is_carry_source<bit, IntT>
constexpr bool carried_out(const IntT lhs, const IntT rhs, const IntT result) {
	// Additive:
	//
	// 0 and 0 => didn't.
	// 0 and 1 or 1 and 0 => did if 0.
	// 1 and 1 => did.
	//
	// Subtractive:
	//
	// 1 and 0 => didn't
	// 1 and 1 or 0 and 0 => did if 1.
	// 0 and 1 => did.
	static_assert(operation == Operation::Add || operation == Operation::Subtract);
	const auto adj_rhs = (operation == Operation::Subtract) ? ~rhs : rhs;
	const bool carry = IntT(1 << bit) & (lhs | adj_rhs) & ((lhs & adj_rhs) | ~result);
	if constexpr (operation == Operation::Subtract) {
		return !carry;
	} else {
		return carry;
	}
}

/// @returns @c true if there was carry into @c bit when computing either:
///		• @c lhs + @c rhs; or
///		• @c lhs - @c rhs;
///	producing @c result.
template <int bit, typename IntT>
requires is_carry_source<bit, IntT>
constexpr bool carried_in(const IntT lhs, const IntT rhs, const IntT result) {
	// 0 and 0 or 1 and 1 => did if 1.
	// 0 and 1 or 1 and 0 => did if 0.
	return IntT(1 << bit) & (lhs ^ rhs ^ result);
}

/// @returns An int of type @c IntT with only the most-significant bit set.
template <typename IntT>
requires std::is_integral_v<IntT> && std::is_unsigned_v<IntT>
constexpr IntT top_bit() {
	static_assert(!std::numeric_limits<IntT>::is_signed);
	constexpr IntT max = std::numeric_limits<IntT>::max();
	return max - (max >> 1);
}

/// @returns The number of bits in @c IntT.
template <typename IntT>
constexpr int bit_size() {
	return sizeof(IntT) * 8;
}

/// @returns An int with the top bit indicating whether overflow occurred during the calculation of
///		• @c lhs + @c rhs (if @c is_add is true); or
///		• @c lhs - @c rhs (if @c is_add is false)
/// and the result was @c result. All other bits will be clear.
template <Operation operation, typename IntT>
requires std::is_integral_v<IntT> && std::is_unsigned_v<IntT>
constexpr IntT overflow(const IntT lhs, const IntT rhs, const IntT result) {
	const IntT output_changed = result ^ lhs;
	const IntT input_differed = lhs ^ rhs;

	static_assert(operation == Operation::Add || operation == Operation::Subtract);
	if constexpr (operation == Operation::Add) {
		return top_bit<IntT>() & output_changed & ~input_differed;
	} else {
		return top_bit<IntT>() & output_changed & input_differed;
	}
}

}
