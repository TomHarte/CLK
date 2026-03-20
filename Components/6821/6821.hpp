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

enum class IRQ {
	A,
	B,
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

			const uint8_t output = ports_[port].output();
			if(output != ports_[port].previous_output) {
				ports_[port].previous_output = output;
				port_handler_.template output<Port(port)>(output);
			}
		}
	}

	template <int address>
	uint8_t read() {
		static constexpr int port = (address >> 1) & 1;
		if constexpr(address & 1) {
			return ports_[port].control;
		} else {
			if(ports_[port].control & Control::DataVisible) {
				return ports_[port].input(port_handler_.template input<Port(port)>());
			} else {
				return ports_[port].direction;
			}
		}
		return 0;
	}

	void refresh() const {
		refresh<Port::A>();
		refresh<Port::B>();
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

		uint8_t output() const {
			return data | ~direction;
		}
		uint8_t input(const uint8_t bus) const {
			return (data & direction) | (bus & ~direction);
		}
		mutable uint8_t previous_output = 0x00;
	} ports_[2];

	template <Port port>
	void refresh() const {
		static constexpr auto index = int(port);

		const uint8_t output = ports_[index].output();
		ports_[index].previous_output = output;
		port_handler_.template output<port>(output);
	}
};

};

#endif /* _821_hpp */
