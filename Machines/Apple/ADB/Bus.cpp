//
//  Bus.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/02/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "Bus.hpp"

using namespace Apple::ADB;

Bus::Bus(HalfCycles clock_speed) : half_cycles_to_microseconds_(1'000'000.0 / clock_speed.as<double>()) {}

void Bus::run_for(HalfCycles duration) {
	time_in_state_ += duration;
	time_since_get_state_ += duration;
}

void Bus::set_device_output(size_t device_id, bool output) {
	// Modify the all-devices bus state.
	bus_state_[device_id] = output;

	// React to signal edges only; don't use get_state here to avoid
	// endless recursion should any reactive devices set new output
	// during the various calls made below.
	const bool data_level = bus_state_.all();
	if(data_level_ != data_level) {
		data_level_ = data_level;

		if(data_level) {
			// This was a transition to high; classify what just happened according to
			// the duration of the low period.
			const double low_microseconds = time_in_state_.as<double>() * half_cycles_to_microseconds_;

			// Low periods:
			// (partly as adapted from the AN591 data sheet; otherwise from the IIgs reference manual)
			//
			//	> 1040 µs		reset
			//	560–1040 µs		attention
			//	< 50 µs			1
			//	50–72 µs		0
			//	300 µs			service request
			if(low_microseconds > 1040.0) {
				for(auto device: devices_) {
					device->adb_bus_did_observe_event(Event::Reset);
				}
			} else if(low_microseconds >= 560.0) {
				for(auto device: devices_) {
					device->adb_bus_did_observe_event(Event::Attention);
				}
				shift_register_ = 1;
			} else if(low_microseconds < 50.0) {
				shift(1);
			} else if(low_microseconds < 72.0) {
				shift(0);
			} else if(low_microseconds >= 291.0 && low_microseconds <= 309.0) {
				for(auto device: devices_) {
					device->adb_bus_did_observe_event(Event::ServiceRequest);
				}
			} else {
				for(auto device: devices_) {
					device->adb_bus_did_observe_event(Event::Unrecognised);
				}
			}
		}

		time_in_state_ = HalfCycles(0);
	}
}

void Bus::shift(unsigned int value) {
	shift_register_ = (shift_register_ << 1) | value;

	// Trigger a byte whenever a start bit hits bit 8.
	if(shift_register_ & 0x100) {
		for(auto device: devices_) {
			device->adb_bus_did_observe_event(Event::Byte, uint8_t(shift_register_));
		}
		shift_register_ = 1;
	}
}

bool Bus::get_state() const {
	const auto microseconds = time_since_get_state_.as<double>() * half_cycles_to_microseconds_;
	time_since_get_state_ = HalfCycles(0);

	const bool current_level = bus_state_.all();
	for(auto device: devices_) {
		device->advance_state(microseconds, current_level);
	}
	return bus_state_.all();
}

size_t Bus::add_device() {
	const size_t id = next_device_id_;
	++next_device_id_;
	return id;
}

size_t Bus::add_device(Device *device) {
	devices_.push_back(device);
	return add_device();
}
