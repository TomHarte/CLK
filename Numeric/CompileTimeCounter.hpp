//
//  CompileTimeCounter.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/10/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

namespace Numeric::Counter {

// Intended usage:
//
//	(1) define a struct to make this count unique; if desired then give it a `static constexpr int base` to
//		dictate the first value returned by next (see SeqBase for an optional helper);
//	(2)	use next<YourTag>() to get the next number in the sequence, current<YourTag>() to get whatever the last
//		call to next returned.
//
// Implementation decision: an initial call to next is expected before calls to current. Calling current before
// any calls to next will result in base - 1.
//
// WARNING: this implementation uses compile-time recursion, so the counter can count up to only 512 on Clang,
// and probably similar limits on other compilers.

template <typename, int> struct CounterImpl {
private:
	struct Count {
		consteval friend bool has_passed(CounterImpl) {
			return true;
		}
	};
	consteval friend bool has_passed(CounterImpl);

public:
	// SFINAE: these functions will exist only if has_passed exists
	// for CounterImpl and the prospective count.
	//
	// Double definition because the fallback cases differ.
	template <typename instantiation = CounterImpl, bool = has_passed(instantiation())>
	static consteval bool test_and_generate(int) {
		return true;
	}

	template <typename instantiation = CounterImpl, bool = has_passed(instantiation())>
	static consteval bool test(int) {
		return true;
	}

	// Fallback cases: possibly generate a new has_passed but otherwise
	// indicate that the prosepctive count is as-yet unused.
	static consteval bool test_and_generate(...) {
		[[maybe_unused]] Count c;
		return false;
	}

	static consteval bool test(...) {
		return false;
	}
};

/// A convenience struct for declaring counter bases locally.
/// @c Owner is purely to give uniqueness.
template <typename Owner, int b>
struct SeqBase {
	static constexpr int base = b;
};

// Both of the below use a tail-recursive loop to find the first instance of
// CounterImpl that indicates that it hasn't yet been counted.

template <typename Seq>
consteval int base() {
	if constexpr (requires {Seq::base;}) {
		return Seq::base;
	}
	return 0;
}

template <typename Seq, int count = 0, typename = decltype([]{})>
consteval int next() {
	if constexpr (!CounterImpl<Seq, count>::test_and_generate(0)) {
		return count + base<Seq>();
	} else {
		return next<Seq, count+1>();
	}
}

template <typename Seq, int count = 0, typename = decltype([]{})>
consteval int current() {
	if constexpr (!CounterImpl<Seq, count>::test(0)) {
		return count + base<Seq>() - 1;
	} else {
		return current<Seq, count+1>();
	}
}

}
