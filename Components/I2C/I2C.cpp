//
//  I2C.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/03/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#include "I2C.hpp"

#include "../../Outputs/Log.hpp"

using namespace I2C;

namespace {

Log::Logger<Log::Source::I2C> logger;

}

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

	logger.info().append("C:%d D:%d", clock_, data_);

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

	const auto await_byte = [&] {
		peripheral_response_ = 0;
		peripheral_bits_ = 2;
		phase_ = Phase::AwaitingByte;
		logger.info().append("Waiting for byte");
	};

	// Check for stop condition at any time.
	// "A LOW-to-HIGH transition of the data line
	// while the clock is HIGH is defined as the STOP condition".
	if(prior_data && !data_ && !clock_) {
		logger.info().append("Stopped");
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
				logger.info().append("Waiting for [remainder of] address");
			}
		break;

		case Phase::CollectingAddress:
			capture_bit();
			if(input_count_ == 8) {
				logger.info().append("Addressed %02x with %s?", uint8_t(input_) & 0xfe, input_ & 1 ? "read" : "write");

				auto pair = peripherals_.find(uint8_t(input_) & 0xfe);
				if(pair != peripherals_.end()) {
					active_peripheral_ = pair->second;

					await_byte();
				} else {
					phase_ = Phase::AwaitingStart;
					logger.info().append("No device; not acknowledging");
				}
			}
		break;

		case Phase::AwaitingByte:
			// Run down the clock on the acknowledge bit.
			if(peripheral_bits_) {
				return;
			}

			logger.info().append("Beginning byte");
			phase_ = Phase::CollectingByte;
			input_count_ = 0;
		[[fallthrough]];

		case Phase::CollectingByte:
			capture_bit();
			if(input_count_ == 8) {
				logger.info().append("Got byte %02x?", uint8_t(input_));

				await_byte();
			}
		break;
	}
}

void Bus::add_peripheral(Peripheral *peripheral, int address) {
	peripherals_[address] = peripheral;
}
