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
	default_adb_device_id_(adb_device_id) {
	reset();
}

void ReactiveDevice::post_response(const std::vector<uint8_t> &&response) {
	response_ = std::move(response);
	microseconds_at_bit_ = 0.0;
	bit_offset_ = -2;
}

void ReactiveDevice::advance_state(double microseconds, bool current_level) {
	// First test: is a service request desired?
	if(phase_ == Phase::ServiceRequestPending) {
		microseconds_at_bit_ += microseconds;
		if(microseconds_at_bit_ < 240.0) {
			bus_.set_device_output(device_id_, false);
		} else {
			bus_.set_device_output(device_id_, true);
			phase_ = Phase::AwaitingAttention;
		}
		return;
	}

	// Do nothing if not in the process of posting a response.
	if(response_.empty()) return;

	/*
		Total process below:

			(1)	assume that the data was enqueued before the stop bit had
				concluded; wait for the end of that;
			(2)	wait for the stop-to-start time period;
			(3)	output a start bit of '1';
			(4)	output all enqueued bytes, MSB to LSB;
			(5)	output a stop bit of '0'; and
			(6)	return this device's output level to high and top.
	*/

	// Wait for the bus to be clear if transmission has not yet begun.
	if(!current_level && bit_offset_ == -2) return;

	// Advance time.
	microseconds_at_bit_ += microseconds;

	// If this is the start of the packet, wait an appropriate stop-to-start time.
	if(bit_offset_ == -2) {
		if(microseconds_at_bit_ < 150.0) {
			return;
		}
		microseconds_at_bit_ -= 150.0;
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
	if(phase_ == Phase::AwaitingAttention) {
		if(event != Bus::Event::Attention) return;
		phase_ = Phase::AwaitingCommand;
		return;
	}

	if(event != Bus::Event::Byte) return;

	if(phase_ == Phase::AwaitingContent) {
		content_.push_back(value);
		if(content_.size() == expected_content_size_) {
			phase_ = Phase::AwaitingAttention;

			if(command_.reg == 3) {
				register3_ = uint16_t((content_[0] << 8) | content_[1]);
			} else {
				did_receive_data(command_, content_);
			}
			content_.clear();
		}
	}

	if(phase_ == Phase::AwaitingCommand) {
		phase_ = Phase::AwaitingAttention;

		command_ = decode_command(value);
//		LOG(command_);

		// If this command doesn't apply here, but a service request is requested,
		// post a service request.
		if(command_.device != Command::AllDevices && command_.device != ((register3_ >> 8) & 0xf)) {
			if(service_desired_) {
				service_desired_ = false;
				stop_has_begin_ = false;
				phase_ = Phase::ServiceRequestPending;
				microseconds_at_bit_ = 0.0;
			}
			return;
		}

		// Handle reset and register 3 here automatically; pass everything else along.
		switch(command_.type) {
			case Command::Type::Reset:
				reset();
			[[fallthrough]];
			default:
				perform_command(command_);
			break;

			case Command::Type::Listen:
			case Command::Type::Talk:
				if(command_.reg == 3) {
					if(command_.type == Command::Type::Talk) {
						post_response({uint8_t(register3_ >> 8), uint8_t(register3_ & 0xff)});
					} else {
						receive_bytes(2);
					}
				} else {
					service_desired_ = false;
					perform_command(command_);
				}
			break;
		}
	}
}

void ReactiveDevice::receive_bytes(size_t count) {
	content_.clear();
	expected_content_size_ = count;
	phase_ = Phase::AwaitingContent;
}

void ReactiveDevice::reset() {
	register3_ = uint16_t(0x6001 | (default_adb_device_id_ << 8));
}

void ReactiveDevice::post_service_request() {
	service_desired_ = true;
}
