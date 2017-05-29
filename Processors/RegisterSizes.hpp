//
//  RegisterSizes.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/05/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef RegisterSizes_hpp
#define RegisterSizes_hpp

#include <cstdint>

namespace CPU {

union RegisterPair {
	RegisterPair(uint16_t v) : full(v) {}
	RegisterPair() {}

	uint16_t full;
	struct {
		uint8_t low, high;
	} bytes;
};

}

#endif /* RegisterSizes_hpp */
