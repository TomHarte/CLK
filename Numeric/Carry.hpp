//
//  Carry.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 30/08/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#ifndef Carry_hpp
#define Carry_hpp

namespace Numeric {

/// @returns @c true if from @c bit there was:
/// 	• carry after calculating @c lhs + @c rhs if @c is_add is true; or
/// 	• borrow after calculating @c lhs - @c rhs if @c is_add is false;
/// producing @c result.
template <bool is_add, int bit, typename IntT> bool carried_out(IntT lhs, IntT rhs, IntT result) {
	// 0 and 0 => didn't.
	// 0 and 1 or 1 and 0 => did if 0.
	// 1 and 1 => did.
	if constexpr (!is_add) {
		rhs = ~rhs;
	}
	const bool carry = IntT(1 << bit) & (lhs | rhs) & ((lhs & rhs) | ~result);
	if constexpr (!is_add) {
		return !carry;
	} else {
		return carry;
	}
}

/// @returns @c true if there was carry into @c bit when computing either:
/// 	• @c lhs + @c rhs; or
/// 	• @c lhs - @c rhs;
///	producing @c result.
template <int bit, typename IntT> bool carried_in(IntT lhs, IntT rhs, IntT result) {
	// 0 and 0 or 1 and 1 => did if 1.
	// 0 and 1 or 1 and 0 => did if 0.
	return IntT(1 << bit) & (lhs ^ rhs ^ result);
}

}

#endif /* Carry_hpp */
