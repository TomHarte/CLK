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
	const bool has_new_bit = clock_pulled && !clock_;
	clock_ = clock_pulled;
	data_ = data_pulled;

	// TODO: "Stop condition: SDA goes high after SCL (?)".

	if(has_new_bit) {
		// Test for a start bit.
		//
		// "Start condition: SDA goes low before SCL".
		if(input_count_ < 0 && data_) {
			input_count_ = 0;
		}

		// Accumulate if started.
		if(input_count_ >= 0) {
			input_ = uint16_t((input_ << 1) | (data_pulled ? 0 : 1));
			++input_count_;
		}

		// Test for meaning.
		switch(phase_) {
			case Phase::AwaitingAddress:
				if(input_count_ == 9) {
					printf("Calling %02x?", uint8_t(input_));

					auto pair = peripherals_.find(uint8_t(input_));
					if(pair != peripherals_.end()) {
						active_peripheral_ = pair->second;
						// TODO: device found; pull data low for the next clock.
					}
				}
			break;

			case Phase::CollectingData:
			break;
		}
	}
}

void Bus::add_peripheral(Peripheral *peripheral, int address) {
	peripherals_[address] = peripheral;
}
