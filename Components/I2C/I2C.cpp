//
//  I2C.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/03/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#include "I2C.hpp"

using namespace I2C;

void Bus::set_data(bool pulled) {
	set_clock_data(clock_, pulled);
}
bool Bus::data() {
	return data_;
}

void Bus::set_clock(bool pulled) {
	set_clock_data(pulled, data_);
}
bool Bus::clock() {
	return clock_;
}

void Bus::set_clock_data(bool clock_pulled, bool data_pulled) {
	// TODO: all intelligence.
	clock_ = clock_pulled;
	data_ = data_pulled;
}

void Bus::add_peripheral(Peripheral *peripheral, int address) {
	peripherals_[address] = peripheral;
}
