//
//  Carry.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 30/08/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#pragma once

#include <limits>

namespace Numeric {

enum class Operation {
	Add,
	Subtract,
};

/// @returns @c true if from @c bit there was:
///		• carry after calculating @c lhs + @c rhs if @c is_add is true; or
///		• borrow after calculating @c lhs - @c rhs if @c is_add is false;
/// producing @c result.
template <Operation operation, int bit, typename IntT> bool carried_out(IntT lhs, IntT rhs, IntT result) {
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
	if constexpr (operation == Operation::Subtract) {
		rhs = ~rhs;
	}
	const bool carry = IntT(1 << bit) & (lhs | rhs) & ((lhs & rhs) | ~result);
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
template <int bit, typename IntT> bool carried_in(IntT lhs, IntT rhs, IntT result) {
	// 0 and 0 or 1 and 1 => did if 1.
	// 0 and 1 or 1 and 0 => did if 0.
	return IntT(1 << bit) & (lhs ^ rhs ^ result);
}

/// @returns An int of type @c IntT with only the most-significant bit set.
template <typename IntT> constexpr IntT top_bit() {
	static_assert(!std::numeric_limits<IntT>::is_signed);
	constexpr IntT max = std::numeric_limits<IntT>::max();
	return max - (max >> 1);
}

/// @returns The number of bits in @c IntT.
template <typename IntT> constexpr int bit_size() {
	return sizeof(IntT) * 8;
}

/// @returns An int with the top bit indicating whether overflow occurred during the calculation of
///		• @c lhs + @c rhs (if @c is_add is true); or
///		• @c lhs - @c rhs (if @c is_add is false)
/// and the result was @c result. All other bits will be clear.
template <Operation operation, typename IntT>
IntT overflow(IntT lhs, IntT rhs, IntT result) {
	const IntT output_changed = result ^ lhs;
	const IntT input_differed = lhs ^ rhs;

	if constexpr (operation == Operation::Add) {
		return top_bit<IntT>() & output_changed & ~input_differed;
	} else {
		return top_bit<IntT>() & output_changed & input_differed;
	}
}

}
