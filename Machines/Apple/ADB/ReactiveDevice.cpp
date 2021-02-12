//
//  ReactiveDevice.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 12/02/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "ReactiveDevice.hpp"

using namespace Apple::ADB;

ReactiveDevice::ReactiveDevice(Apple::ADB::Bus &bus) : bus_(bus), device_id_(bus.add_device(this)) {}

void ReactiveDevice::post_response(const std::vector<uint8_t> &&response) {
	response_ = std::move(response);
	microseconds_at_bit_ = 0.0;
	bit_offset_ = -1;
}

void ReactiveDevice::advance_state(double microseconds) {
	// Do nothing if not in the process of posting a response.
	if(response_.empty()) return;

	// Otherwise advance time appropriately.
	microseconds_at_bit_ += microseconds;

	// Bit '-1' is the sync signal.
	if(bit_offset_ == -1) {
		bus_.set_device_output(device_id_, false);
		if(microseconds_at_bit_ < 300) {
			return;
		}
		microseconds_at_bit_ -= 300;
		++bit_offset_;
	}

	// Advance the implied number of bits.
	bit_offset_ += int(microseconds_at_bit_ / 100);
	microseconds_at_bit_ -= double(bit_offset_ * 100.0);

	// Check for end-of-transmission.
	if(bit_offset_ >= int(response_.size() * 10)) {
		bus_.set_device_output(device_id_, true);
		response_.clear();
		return;
	}

	// Otherwise pick an output level.
	const int byte = bit_offset_ / 10;
	const int bit = ((0x200 | (int(response_[size_t(byte)]) << 1)) >> (bit_offset_ % 10)) & 1;

	constexpr double low_periods[] = {66, 33};
	bus_.set_device_output(device_id_, microseconds_at_bit_ > low_periods[bit]);
}
