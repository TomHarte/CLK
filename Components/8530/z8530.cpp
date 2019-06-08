//
//  8530.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/06/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "z8530.hpp"

using namespace Zilog::SCC;

void z8530::reset() {
}

std::uint8_t z8530::read(int address) {
	return channels_[address & 1].read(address & 2);
}

void z8530::write(int address, std::uint8_t value) {
	channels_[address & 1].write(address & 2, pointer_, value);

	/*
		Control register 0, which includes the pointer bits,
		is decoded here because there's only one set of pointer bits.
	*/
	if(!(address & 2)) {
		if(pointer_) {
			pointer_ = 0;
		} else {
			pointer_ = value & 7;
		}
	}
}

uint8_t z8530::Channel::read(bool data) {
	// If this is a data read, just return it.
	if(data) return data_;

	// Otherwise, this is a control read...
	return 0x00;
}

void z8530::Channel::write(bool data, uint8_t pointer, uint8_t value) {
	if(data) {
		data_ = value;
		return;
	}
}
