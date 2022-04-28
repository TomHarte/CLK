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

namespace CPU {

/// Provides a union that — on most compilers for modern consumer architectures,
/// and therefore this project assumes universally — provides access to the low and
/// high halves of some larger integer type.
template <typename Full, typename Half> union RegisterPair {
	RegisterPair(Full v) : full(v) {}
	RegisterPair() {}

	Full full;
#pragma pack(push, 1)
#if TARGET_RT_BIG_ENDIAN
	struct {
		Half high, low;
	} halves;
#else
	struct {
		Half low, high;
	} halves;
#endif
#pragma pack(pop)
};

typedef RegisterPair<uint16_t, uint8_t> RegisterPair16;
typedef RegisterPair<uint32_t, RegisterPair16> RegisterPair32;

}

#endif /* RegisterSizes_hpp */
