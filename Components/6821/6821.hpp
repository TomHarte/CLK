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

template <typename BusHandlerT>
class MC6821 {
public:
	template <int address>
	void write(const uint8_t value) {
		static constexpr int port = (address >> 1) & 1;

		if constexpr (address & 1) {
			ports_[port].control = value;
		} else {
			if(ports_[port].control & Control::DataVisible) {
				ports_[port].data = value;
			} else {
				ports_[port].direction = value;
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
				return
					(ports_[port].data & ports_[port].direction) |
					(ports_[port].input & ~ports_[port].direction);
			} else {
				return ports_[port].direction;
			}
		}
		return 0;
	}

private:
	enum Control: uint8_t {
		IRQA			= 0b1000'0000,
		IRQB			= 0b0100'0000,
		CA2				= 0b0011'1000,
		DataVisible		= 0b0000'0100,
		CA1				= 0b0000'0011,
	};

	struct Port {
		uint8_t control;
		uint8_t data;
		uint8_t direction;
		uint8_t input = 0xff;
	} ports_[2];
};

};

#endif /* _821_hpp */
