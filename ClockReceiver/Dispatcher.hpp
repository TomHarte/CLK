//
//  Serialiser.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/05/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#ifndef Dispatcher_hpp
#define Dispatcherr_hpp

namespace Dispatcher {

/// The unity function; converts n directly to n.
struct UnitConverter {
	constexpr int operator ()(int n) {
		return n;
	}
};

template <int max, typename SequencerT, typename ConverterT = UnitConverter>
struct Dispatcher {

	/// Perform @c target.perform<n>() for the input range [start, end]; @c ConverterT()(n) will be applied to
	/// each individual step before it becomes the relevant template argument.
	void dispatch(SequencerT &target, int start, int end) {

		// Minor optimisation: do a comparison with end once outside the loop and if it implies so
		// then do no further comparisons within the loop. This is somewhat targetted at expected
		// use cases.
		if(end < max) {
			dispatch<true>(target, start, end);
		} else {
			dispatch<false>(target, start, end);
		}
	}

private:
	template <bool use_end> void dispatch(SequencerT &target, int start, int end) {
		static_assert(max < 2048);

		// Yes, macros, yuck. But I want an actual switch statement for the dispatch to start
		// and to allow a simple [[fallthrough]] through all subsequent steps up until end.
		// So I don't think I have much in the way of options here.
		//
		// Sensible choices by the optimiser are assumed.

#define index(n)							\
	if constexpr (n == max) return;			\
	if(use_end && end == n) return;			\
	[[fallthrough]];						\
	case n: target.template perform<ConverterT()(n)>();

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

}

#endif /* Dispatcher_hpp */
