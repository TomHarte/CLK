//
//  UpperBound.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 11/12/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#pragma once

namespace Numeric {

/// @returns The element that is `index - offset` into the list given by the rest of the
/// variadic arguments or the final element if `offset` is out of bounds.
///
/// E.g. @c at_index<0, 3, 5, 6, 7, 8, 9>() returns the `3 - 0` = 4th element from the
/// list 5, 6, 7, 8, 9, i.e. 8.
template<int origin, int index, int T, int... Args>
constexpr int at_index() {
	if constexpr (origin == index || sizeof...(Args) == 0) {
		return T;
	} else {
		return at_index<origin + 1, index, Args...>();
	}
}

/// @returns The result of binary searching for the first thing in the range `[left, right)` within
/// the other template arguments that is strictly greater than @c location.
template <int left, int right, int... Args>
int upper_bound_bounded(int location) {
	if constexpr (left + 1 == right) {
		return at_index<0, left+1, Args...>();
	}

	static constexpr auto midpoint = (left + right) >> 1;
	if(location >= at_index<0, midpoint, Args...>()) {
		return upper_bound_bounded<midpoint, right, Args...>(location);
	} else {
		return upper_bound_bounded<left, midpoint, Args...>(location);
	}
}

template <int index, int... Args>
constexpr int is_ordered() {
	if constexpr (sizeof...(Args) == index + 1) {
		return true;
	} else {
		return
			(at_index<0, index, Args...>() < at_index<0, index+1, Args...>()) &&
			is_ordered<index+1, Args...>();
	}
}

/// @returns The result of binary searching for the first thing in the template arguments
/// is strictly greater than @c location.
template <int... Args>
int upper_bound(int location) {
	static_assert(is_ordered<0, Args...>(), "Template arguments must be in ascending order.");
	return upper_bound_bounded<0, sizeof...(Args), Args...>(location);
}

}
