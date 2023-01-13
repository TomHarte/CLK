//
//  RegisterSizes.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/05/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef RegisterSizes_hpp
#define RegisterSizes_hpp

#include <cstdint>

#pragma pack(push, 1)

// Unions below assume a modern consumer architecture,
// and that this compiler offers C-style anonymous structs.

namespace CPU {

/// Provides access to all intermediate parts of a larger int.
template <typename Full, typename Half> union alignas(Full) RegisterPair {
	RegisterPair(Full v) : full(v) {}
	RegisterPair() {}

	Full full;
#if TARGET_RT_BIG_ENDIAN
	struct {
		Half high, low;
	} halves;
#else
	struct {
		Half low, high;
	} halves;
#endif
};

using RegisterPair16 = RegisterPair<uint16_t, uint8_t>;
using RegisterPair32 = RegisterPair<uint32_t, RegisterPair16>;

/// Provides access to lower portions of a larger int.
template <typename IntT> union SlicedInt {};

template <> union SlicedInt<uint16_t> {
	struct {
		uint16_t w;
	};

	struct {
#if TARGET_RT_BIG_ENDIAN
		uint8_t __padding[1];
#endif
		uint8_t b;
	};
};

template <> union SlicedInt<uint32_t> {
	struct {
		uint32_t l;
	};

	struct {
#if TARGET_RT_BIG_ENDIAN
		uint16_t __padding[1];
#endif
		uint16_t w;
	};

	struct {
#if TARGET_RT_BIG_ENDIAN
		uint8_t __padding[3];
#endif
		uint8_t b;
	};

	struct {
#if TARGET_RT_BIG_ENDIAN
		SlicedInt<uint16_t> high, low;
#else
		SlicedInt<uint16_t> low, high;
#endif
	};
};

using SlicedInt16 = SlicedInt<uint16_t>;
using SlicedInt32 = SlicedInt<uint32_t>;

}

#pragma pack(pop)

#endif /* RegisterSizes_hpp */
