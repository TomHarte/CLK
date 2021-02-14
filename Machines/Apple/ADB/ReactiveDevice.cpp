//
//  ReactiveDevice.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 12/02/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "ReactiveDevice.hpp"

#define LOG_PREFIX "[ADB device] "
#include "../../../Outputs/Log.hpp"

using namespace Apple::ADB;

ReactiveDevice::ReactiveDevice(Apple::ADB::Bus &bus, uint8_t adb_device_id) :
	bus_(bus),
	device_id_(bus.add_device(this)),
	adb_device_id_(adb_device_id) {}

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
	const int step = int(microseconds_at_bit_ / 100);
	bit_offset_ += step;
	microseconds_at_bit_ -= double(step * 100.0);

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

void ReactiveDevice::adb_bus_did_observe_event(Bus::Event event, uint8_t value) {
	if(!next_is_command_ && event != Bus::Event::Attention) {
		return;
	}

	if(next_is_command_ && event == Bus::Event::Byte) {
		next_is_command_ = false;

		const auto command = decode_command(value);
		LOG(command);
		if(command.device == adb_device_id_) {
			// TODO: handle fixed commands here (like register 3?)
			perform_command(command);
		}
	} else if(event == Bus::Event::Attention) {
		next_is_command_ = true;
	}
}
