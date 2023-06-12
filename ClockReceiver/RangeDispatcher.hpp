//
//  RangeDispatcher.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/05/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#ifndef RangeDispatcher_hpp
#define RangeDispatcher_hpp

#include <algorithm>

namespace Reflection {

/// Provides glue for a run of calls like:
///
/// 	SequencerT.perform<0>(...)
/// 	SequencerT.perform<1>(...)
/// 	SequencerT.perform<2>(...)
/// 	...etc...
///
/// Allowing the caller to execute any subrange of the calls.
template <typename SequencerT>
struct RangeDispatcher {

	/// Perform @c target.perform<n>() for the input range `start <= n < end`.
	template <typename... Args>
	static void dispatch(SequencerT &target, int start, int end, Args&&... args) {

		// Minor optimisation: do a comparison with end once outside the loop and if it implies so
		// then do no further comparisons within the loop. This is somewhat targetted at expected
		// use cases.
		if(end < SequencerT::max) {
			dispatch<true>(target, start, end, args...);
		} else {
			dispatch<false>(target, start, end, args...);
		}
	}

private:
	template <bool use_end, typename... Args> static void dispatch(SequencerT &target, int start, int end, Args&&... args) {
		static_assert(SequencerT::max < 2048);

		// Yes, macros, yuck. But I want an actual switch statement for the dispatch to start
		// and to allow a simple [[fallthrough]] through all subsequent steps up until end.
		// So I don't think I have much in the way of options here.
		//
		// Sensible choices by the optimiser are assumed.

#define index(n)												\
	case n:														\
		if constexpr (n <= SequencerT::max) {					\
			if constexpr (n == SequencerT::max) return;			\
			if constexpr (n < SequencerT::max) {				\
				if(use_end && end == n) return;					\
			}													\
			target.template perform<n>(start, end, args...);	\
		}														\
	[[fallthrough]];

#define index2(n)		index(n);		index(n+1);
#define index4(n)		index2(n);		index2(n+2);
#define index8(n)		index4(n);		index4(n+4);
#define index16(n)		index8(n);		index8(n+8);
#define index32(n)		index16(n);		index16(n+16);
#define index64(n)		index32(n);		index32(n+32);
#define index128(n)		index64(n);		index64(n+64);
#define index256(n)		index128(n);	index128(n+128);
#define index512(n)		index256(n);	index256(n+256);
#define index1024(n)	index512(n);	index512(n+512);
#define index2048(n)	index1024(n);	index1024(n+1024);

		switch(start) {
			default: 	assert(false);
			index2048(0);
		}

#undef index
#undef index2
#undef index4
#undef index8
#undef index16
#undef index32
#undef index64
#undef index128
#undef index256
#undef index512
#undef index1024
#undef index2048
	}

};

/// An optional target for a RangeDispatcher which uses a classifier to divide the input region into typed ranges, issuing calls to the target
/// only to begin and end each subrange, and for the number of cycles spent within.
template <typename ClassifierT, typename TargetT>
struct SubrangeDispatcher {
	static constexpr int max = ClassifierT::max;

	template <int n>
	void perform(int begin, int end) {
		constexpr auto region = ClassifierT::region(n);
		const auto clipped_start = std::max(begin, find_begin(n));
		const auto clipped_end = std::min(end, find_end(n));

		if constexpr (n == find_begin(n)) {
			target.begin<region>(clipped_start);
		}

		target.advance<region>(clipped_end - clipped_start);

		if constexpr (n + 1 == find_end(n)) {
			target.end<region>(clipped_end);
		}
	}

	private:
	constexpr int find_begin(int n) {
		const auto type = ClassifierT::region(n);
		while(n && ClassifierT::region(n - 1) == type) --n;
		return n;
	}

	constexpr int find_end(int n) {
		const auto type = ClassifierT::region(n);
		while(n < ClassifierT::max && ClassifierT::region(n) == type) ++n;
		return n;
	}

	TargetT &target;
};

}

#endif /* RangeDispatcher_hpp */
