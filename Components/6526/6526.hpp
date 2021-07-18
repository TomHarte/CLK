//
//  6526.h
//  Clock Signal
//
//  Created by Thomas Harte on 18/07/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef _526_h
#define _526_h

#include <cstdint>

namespace MOS {
namespace MOS6526 {

class PortHandler {

};

enum class Personality {
	// The 6526, used in machines such as the C64, has a BCD time-of-day clock.
	Model6526,
	// The 8250, used in the Amiga, provides a binary time-of-day clock.
	Model8250,
};

template <typename BusHandlerT, Personality personality> class MOS6526 {
	public:
		/// Writes @c value to the register at @c address. Only the low two bits of the address are decoded.
		void write(int address, uint8_t value);

		/// Fetches the value of the register @c address. Only the low two bits of the address are decoded.
		uint8_t read(int address);
};

}
}

#include "Implementation/6526Implementation.hpp"

#endif /* _526_h */
