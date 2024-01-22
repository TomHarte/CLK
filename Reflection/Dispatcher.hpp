//
//  Dispatcher.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/03/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#pragma once

#include <algorithm>
#include <cstdint>

namespace Reflection {

/*!
	Calls @c t.dispatch<c>(args...) as efficiently as possible.
*/
template <typename TargetT, typename... Args> void dispatch(TargetT &t, uint8_t c, Args &&... args) {
#define Opt(x)		case x: t.template dispatch<x>(std::forward<Args>(args)...);	break
#define Opt4(x)		Opt(x + 0x00);		Opt(x + 0x01);		Opt(x + 0x02);		Opt(x + 0x03)
#define Opt16(x)	Opt4(x + 0x00);		Opt4(x + 0x04);		Opt4(x + 0x08);		Opt4(x + 0x0c)
#define Opt64(x)	Opt16(x + 0x00);	Opt16(x + 0x10);	Opt16(x + 0x20);	Opt16(x + 0x30)
#define Opt256(x)	Opt64(x + 0x00);	Opt64(x + 0x40);	Opt64(x + 0x80);	Opt64(x + 0xc0)

	switch(c) {
		Opt256(0);
	}

#undef Opt256
#undef Opt64
#undef Opt16
#undef Opt4
#undef Opt
}

// Yes, macros, yuck. But I want an actual switch statement for the dispatch to start
// and to allow a simple [[fallthrough]] through all subsequent steps up until end.
// So I don't think I have much in the way of options here.
//
// Sensible choices by the optimiser are assumed.
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

#define switch_indices(x)	switch(x) { default: assert(false); index2048(0); }
static constexpr int switch_max = 2048;

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
	static_assert(SequencerT::max < switch_max);

	/// Perform @c target.perform<n>() for the input range `begin <= n < end`.
	template <typename... Args>
	static void dispatch(SequencerT &target, int begin, int end, Args&&... args) {

		// Minor optimisation: do a comparison with end once outside the loop and if it implies so
		// then do no further comparisons within the loop. This is somewhat targetted at expected
		// use cases.
		if(end < SequencerT::max) {
			dispatch<true>(target, begin, end, args...);
		} else {
			dispatch<false>(target, begin, end, args...);
		}
	}

	private:
		template <bool use_end, typename... Args> static void dispatch(SequencerT &target, int begin, int end, Args&&... args) {
#define index(n)											\
	case n:													\
		if constexpr (n <= SequencerT::max) {				\
			if constexpr (n == SequencerT::max) return;		\
			if constexpr (n < SequencerT::max) {			\
				if(use_end && end == n) return;				\
			}												\
			target.template perform<n>(args...);			\
		}													\
	[[fallthrough]];

			switch_indices(begin);

#undef index
		}

};

/// Uses a classifier to divide a range into typed subranges and issues calls to a target of the form:
///
/// * begin<type>(location) upon entering a new region;
/// * end<type>(location) upon entering a region; and
/// * advance<type>(distance) in as many chunks as required to populate the inside of the region.
///
/// @c begin and @c end have iterator-style semantics: begin's location will be the first location in the relevant subrange and
/// end's will be the first location not in the relevant subrange.
template <typename ClassifierT, typename TargetT>
struct SubrangeDispatcher {
	static_assert(ClassifierT::max < switch_max);

	static void dispatch(TargetT &target, int begin, int end) {
#define index(n)																\
	case n:																		\
		if constexpr (n <= ClassifierT::max) {									\
			constexpr auto region = ClassifierT::region(n);						\
			if constexpr (n == find_begin(n)) {									\
				if(n >= end) {													\
					return;														\
				}																\
				target.template begin<region>(n);								\
			}																	\
			if constexpr (n == find_end(n) - 1) {								\
				const auto clipped_begin = std::max(begin, find_begin(n));		\
				const auto clipped_end = std::min(end, find_end(n));			\
				target.template advance<region>(clipped_end - clipped_begin);	\
																				\
				if(clipped_end == n + 1) {										\
					target.template end<region>(n + 1);							\
				}																\
			}																	\
		}																		\
	[[fallthrough]];

			switch_indices(begin);

#undef index
	}

	private:
		static constexpr int find_begin(int n) {
			const auto type = ClassifierT::region(n);
			while(n && ClassifierT::region(n - 1) == type) --n;
			return n;
		}

		static constexpr int find_end(int n) {
			const auto type = ClassifierT::region(n);
			while(n < ClassifierT::max && ClassifierT::region(n) == type) ++n;
			return n;
		}
};

#undef switch_indices

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
