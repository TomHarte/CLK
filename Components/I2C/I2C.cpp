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
	bool result = data_;
	if(peripheral_bits_) {
		result |= !(peripheral_response_ & 0x80);
	}
	return result;
}

void Bus::set_clock(bool pulled) {
	set_clock_data(pulled, data_);
}
bool Bus::clock() {
	return clock_;
}

void Bus::set_clock_data(bool clock_pulled, bool data_pulled) {
	const bool prior_clock = clock_;
	const bool prior_data = data_;
	clock_ = clock_pulled;
	data_ = data_pulled;

	// Advance peripheral input from peripheral if clock
	// transitions from high to low.
	if(peripheral_bits_ && prior_clock && !clock_) {
		--peripheral_bits_;
		peripheral_response_ <<= 1;
	}

	const auto capture_bit = [&]() {
		if(prior_clock && !clock_) {
			input_ = uint16_t((input_ << 1) | (data_pulled ? 0 : 1));
			++input_count_;
		}
	};

	// Check for stop condition at any time.
	// "A LOW-to-HIGH transition of the data line
	// while the clock is HIGH is defined as the STOP condition".
	if(prior_data && !data_ && !clock_) {
		printf("Stopped\n");
		phase_ = Phase::AwaitingStart;
	}

	switch(phase_) {
		case Phase::AwaitingStart:
			// "A HIGH-to-LOW transition of the data line while
			// the clock is HIGH is defined as the START condition"
			if(!prior_data && data_ && !clock_) {
				phase_ = Phase::CollectingAddress;
				input_count_ = 0;
				input_ = 0;
				printf("Waiting for [remainder of] address\n");
			}
		break;

		case Phase::CollectingAddress:
			capture_bit();
			if(input_count_ == 8) {
				printf("Addressed %02x?\n", uint8_t(input_));

				auto pair = peripherals_.find(uint8_t(input_));
				if(pair != peripherals_.end()) {
					active_peripheral_ = pair->second;

					peripheral_response_ = 0;
					peripheral_bits_ = 2;
					phase_ = Phase::AwaitingByte;
					printf("Waiting for byte\n");
				} else {
					phase_ = Phase::AwaitingStart;
					printf("No device\n");
				}
			}
		break;

		case Phase::AwaitingByte:
			if(data_ && clock_) {
				printf("Beginning byte\n");
				phase_ = Phase::CollectingByte;
				input_count_ = 0;
				input_ = 0;
			}
		break;

		case Phase::CollectingByte:
			capture_bit();
			if(input_count_ == 8) {
				printf("Got byte %02x?\n", uint8_t(input_));
				phase_ = Phase::AwaitingByte;
			}
		break;
	}
}

void Bus::add_peripheral(Peripheral *peripheral, int address) {
	peripherals_[address] = peripheral;
}
