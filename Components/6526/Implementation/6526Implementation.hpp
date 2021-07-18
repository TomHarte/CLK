//
//  6526Implementation.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/07/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef _526Implementation_h
#define _526Implementation_h

namespace MOS {
namespace MOS6526 {

template <typename BusHandlerT, Personality personality>
void MOS6526<BusHandlerT, personality>::write(int address, uint8_t value) {
	(void)address;
	(void)value;
}

template <typename BusHandlerT, Personality personality>
uint8_t MOS6526<BusHandlerT, personality>::read(int address) {
	(void)address;
	return 0xff;
}

template <typename BusHandlerT, Personality personality>
void MOS6526<BusHandlerT, personality>::run_for(const HalfCycles half_cycles) {
	(void)half_cycles;
}

}
}

#endif /* _526Implementation_h */
