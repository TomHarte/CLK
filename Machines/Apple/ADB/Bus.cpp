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
}

size_t Bus::add_device() {
	const size_t id = next_device_id_;
	++next_device_id_;
	return id;
}

void Bus::set_device_output(size_t device, bool output) {
	// Modify the all-devices bus state.
	bus_state_[device] = output;

	// React to signal edges only.
	const bool data_level = get_state();
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
				for(auto observer: observers_) {
					observer->adb_bus_did_observe_event(this, Event::Reset);
				}
			} else if(low_microseconds >= 560.0) {
				for(auto observer: observers_) {
					observer->adb_bus_did_observe_event(this, Event::Attention);
				}
				shift_register_ = 1;
			} else if(low_microseconds < 50.0) {
				shift(1);
			} else if(low_microseconds < 72.0) {
				shift(0);
			} else if(low_microseconds >= 291.0 && low_microseconds <= 309.0) {
				for(auto observer: observers_) {
					observer->adb_bus_did_observe_event(this, Event::ServiceRequest);
				}
			} else {
				for(auto observer: observers_) {
					observer->adb_bus_did_observe_event(this, Event::Unrecognised);
				}
			}
		}

		time_in_state_ = HalfCycles(0);
	}
}

void Bus::shift(unsigned int value) {
	shift_register_ = (shift_register_ << 1) | value;

	// Trigger a byte whenever a start bit hits bit 9.
	if(shift_register_ & 0x200) {
		for(auto observer: observers_) {
			observer->adb_bus_did_observe_event(this, Event::Byte, uint8_t(shift_register_ >> 1));
		}
		shift_register_ = 0;
	}
}

bool Bus::get_state() const {
	return bus_state_.all();
}

void Bus::add_observer(Observer *observer) {
	observers_.push_back(observer);
}
