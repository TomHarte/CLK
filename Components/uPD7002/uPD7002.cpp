//
//  uPD7002.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/09/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#include "uPD7002.hpp"

using namespace NEC;

uPD7002::uPD7002(const HalfCycles clock_rate) {
	// Per the BBC AUG: "8 bit conversions typically take 4 ms to complete whereas 10 bit
	// conversions typically take 10 ms to complete".
	fast_period_ = clock_rate / 250;
	slow_period_ = clock_rate / 100;
}

void uPD7002::run_for(const HalfCycles count) {
	if(!conversion_time_remaining_) {
		return;
	}

	if(count >= conversion_time_remaining_) {
		conversion_time_remaining_ = HalfCycles(0);
		result_ = uint16_t(inputs_[channel_] * 65535.0f) & (high_precision_ ? 0xfff0 : 0xff00);
		set_interrupt(true);
		return;
	}

	conversion_time_remaining_ -= count;
}

bool uPD7002::interrupt() const {
	return interrupt_;
}

void uPD7002::write(const uint16_t address, const uint8_t value) {
	const auto local_address = address & 3;
	if(!local_address) {
		channel_ = value & 0b0000'0011;
		spare_ = value & 0b0000'0100;
		high_precision_ = value & 0b0000'1000;
		conversion_time_remaining_ = high_precision_ ? slow_period_ : fast_period_;
		set_interrupt(false);
		return;
	}
}

uint8_t uPD7002::read(const uint16_t address) {
	switch(address & 3) {
		default:
		case 0:	return status();
		case 1:
			set_interrupt(false);
		return uint8_t(result_ >> 8);
		case 2:	return uint8_t(result_);
		case 3:	return 0xff;
	}
}

uint8_t uPD7002::status() const {
	return
		channel_ |
		spare_ |
		(high_precision_ ? 0x08 : 0) |
		((result_ >> 14) & 0x30) |
		(conversion_time_remaining_ > HalfCycles(0) ? 0x00 : 0x40) |
		(interrupt_ ? 0x00 : 0x80);
}

void uPD7002::set_delegate(Delegate *const delegate) {
	delegate_ = delegate;
}

void uPD7002::set_interrupt(const bool value) {
	if(interrupt_ == value) return;
	interrupt_ = value;
	if(delegate_) delegate_->did_change_interrupt_status(*this);
}

void uPD7002::set_input(const int channel, const float value) {
	inputs_[channel] = value;
}
