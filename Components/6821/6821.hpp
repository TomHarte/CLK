//
//  6821.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 19/03/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#ifndef _821_hpp
#define _821_hpp

namespace Motorola::MC6821 {

enum class Port {
	A = 0,
	B = 1,
};

template <typename PortHandlerT, int RS0Mask = 1, int RS1Mask = 2>
class MC6821 {
public:
	MC6821(PortHandlerT &port_handler) : port_handler_(port_handler) {}

	template <int address>
	void write(const uint8_t value) {
		static constexpr int port = bool(address & RS1Mask);

		if constexpr (address & RS0Mask) {
			ports_[port].control = value;
		} else {
			if(ports_[port].control & Control::DataVisible) {
				ports_[port].data = value;
			} else {
				ports_[port].direction = value;
			}
			port_handler_.template output<Port(port)>(
				ports_[port].data | (~ports_[port].direction)
			);
		}

//		if(!port) {
			printf("[%04x %d [%d]]: %d: %02x; c:%02x d:%02x dir:%02x; output: %02x\n",
				uint16_t(address), port, bool(ports_[port].control & Control::DataVisible),
				bool(address & RS0Mask), value,
				ports_[port].control, ports_[port].data, ports_[port].direction,
				uint8_t(ports_[port].data | (~ports_[port].direction))
			);
//		}
	}

	template <int address>
	uint8_t read() {
		static constexpr int port = (address >> 1) & 1;
		if constexpr(address & 1) {
			return ports_[port].control;
		} else {
			if(ports_[port].control & Control::DataVisible) {
				return
					(ports_[port].data & ports_[port].direction) |
					(port_handler_.template input<Port(port)>() & ~ports_[port].direction);
			} else {
				return ports_[port].direction;
			}
		}
		return 0;
	}

private:
	PortHandlerT &port_handler_;

	enum Control: uint8_t {
		IRQA			= 0b1000'0000,
		IRQB			= 0b0100'0000,
		CA2				= 0b0011'1000,
		DataVisible		= 0b0000'0100,
		CA1				= 0b0000'0011,
	};

	struct {
		uint8_t control = 0;
		uint8_t data = 0;
		uint8_t direction = 0;	// Per bit: 0 = input; 1 = output.
	} ports_[2];
};

};

#endif /* _821_hpp */
