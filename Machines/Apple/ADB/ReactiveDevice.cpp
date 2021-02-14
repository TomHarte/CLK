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
	bit_offset_ = -2;
}

void ReactiveDevice::advance_state(double microseconds, bool current_level) {
	// Do nothing if not in the process of posting a response.
	if(response_.empty()) return;

	// Wait for the bus to be clear if transmission has not yet begun.
	if(!current_level && bit_offset_ == -2) return;

	// Advance time.
	microseconds_at_bit_ += microseconds;

	// If this is the start of the packet, wait an appropriate stop-to-start time.
	if(bit_offset_ == -2) {
		if(microseconds_at_bit_ < 250.0) {
			return;
		}
		microseconds_at_bit_ -= 250.0;
		++bit_offset_;
	}

	// Advance the implied number of bits.
	const int step = int(microseconds_at_bit_ / 100.0);
	bit_offset_ += step;
	microseconds_at_bit_ -= double(step * 100.0);

	// Check for end-of-transmission.
	const int response_bit_length = int(response_.size() * 8);
	if(bit_offset_ >= 1 + response_bit_length) {
		bus_.set_device_output(device_id_, true);
		response_.clear();
		return;
	}

	// Otherwise pick the bit to output: it'll either be the start bit of 1,
	// from the provided data, or a stop bit of 0.
	int bit = 0;
	if(bit_offset_ < 0) {
		bit = 1;
	} else if(bit_offset_ < response_bit_length) {
		const int byte = bit_offset_ >> 3;
		const int packet = int(response_[size_t(byte)]);
		bit = (packet >> (7 - (bit_offset_ & 7))) & 1;
	}

	// Convert that into a level.
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

void ReactiveDevice::post_service_request() {
	// TODO.
}
