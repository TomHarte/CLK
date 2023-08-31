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

/// @returns @c true if there was carry out of @c bit when @c source1 and @c source2 were added, producing @c result.
template <int bit, typename IntT> bool carried_out(IntT source1, IntT source2, IntT result) {
	// 0 and 0 => didn't.
	// 0 and 1 or 1 and 0 => did if 0.
	// 1 and 1 => did.
	return IntT(1 << bit) & (source1 | source2) & ((source1 & source2) | ~result);
}

/// @returns @c true if there was carry into @c bit when @c source1 and @c source2 were added, producing @c result.
template <int bit, typename IntT> bool carried_in(IntT source1, IntT source2, IntT result) {
	// 0 and 0 or 1 and 1 => did if 1
	// 0 and 1 or 1 and 0 => did if 0
	return IntT(1 << bit) & (source1 ^ source2 ^ result);
}

}

#endif /* Carry_hpp */
