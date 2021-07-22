//
//  6526Implementation.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/07/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef _526Implementation_h
#define _526Implementation_h

#include <cassert>
#include <cstdio>

namespace MOS {
namespace MOS6526 {

template <typename BusHandlerT, Personality personality>
template <int port> void MOS6526<BusHandlerT, personality>::set_port_output() {
	const uint8_t output = registers_.output[port] | (~registers_.data_direction[port]);
	port_handler_.set_port_output(Port(port), output);
}

template <typename BusHandlerT, Personality personality>
template <int port> uint8_t MOS6526<BusHandlerT, personality>::get_port_input() {
	const uint8_t input = port_handler_.get_port_input(Port(port));
	return (input & ~registers_.data_direction[port]) | (registers_.output[port] & registers_.data_direction[port]);
}

template <typename BusHandlerT, Personality personality>
void MOS6526<BusHandlerT, personality>::update_interrupts() {
}

template <typename BusHandlerT, Personality personality>
void MOS6526<BusHandlerT, personality>::write(int address, uint8_t value) {
	address &= 0xf;
	switch(address) {
		// Port output.
		case 0:
			registers_.output[0] = value;
			set_port_output<0>();
		break;
		case 1:
			registers_.output[1] = value;
			set_port_output<1>();
		break;

		// Port direction.
		case 2:
			registers_.data_direction[0] = value;
			set_port_output<0>();
		break;
		case 3:
			registers_.data_direction[1] = value;
			set_port_output<1>();
		break;

		// Interrupt control.
		case 13:
			registers_.interrupt_control_ = value;
			update_interrupts();
		break;

		default:
			printf("Unhandled 6526 write: %02x to %d\n", value, address);
			assert(false);
		break;
	}
}

template <typename BusHandlerT, Personality personality>
uint8_t MOS6526<BusHandlerT, personality>::read(int address) {
	address &= 0xf;
	switch(address) {
		case 0:		return get_port_input<0>();
		case 1:		return get_port_input<1>();
		case 13:	return registers_.interrupt_control_;

		case 2: case 3:
		return registers_.data_direction[address - 2];


		default:
			printf("Unhandled 6526 read from %d\n", address);
			assert(false);
		break;
	}
	return 0xff;
}

template <typename BusHandlerT, Personality personality>
void MOS6526<BusHandlerT, personality>::run_for(const HalfCycles half_cycles) {
	(void)half_cycles;
}

}
}

#endif /* _526Implementation_h */
