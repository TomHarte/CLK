//
//  SCSI.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 12/08/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "SCSI.hpp"

using namespace SCSI;

Bus::Bus(HalfCycles clock_rate) {
	cycles_to_time_ = 1.0 / double(clock_rate.as_int());

	// NB: note that the dispatch times below are **ORDERED**
	// from least to greatest. Each box should contain the number
	// of whole clock periods it will take to get the the first
	// discrete moment after the required delay interval has been met.
	dispatch_times_[0] = 1 + int(CableSkew / cycles_to_time_);
	dispatch_times_[1] = 1 + int(DeskewDelay / cycles_to_time_);
	dispatch_times_[2] = 1 + int(BusFreeDelay / cycles_to_time_);
	dispatch_times_[3] = 1 + int(BusSettleDelay / cycles_to_time_);
	dispatch_times_[4] = 1 + int(BusClearDelay / cycles_to_time_);
	dispatch_times_[5] = 1 + int(BusSetDelay / cycles_to_time_);
	dispatch_times_[6] = 1 + int(ArbitrationDelay / cycles_to_time_);
	dispatch_times_[7] = 1 + int(ResetHoldTime / cycles_to_time_);
}

size_t Bus::add_device() {
	const auto slot = device_states_.size();
	device_states_.push_back(DefaultBusState);
	return slot;
}

void Bus::set_device_output(size_t device, BusState output) {
	if(device_states_[device] == output) return;
	device_states_[device] = output;

	const auto previous_state = state_;
	state_ = DefaultBusState;
	for(auto state: device_states_) {
		state_ |= state;
	}
	if(state_ == previous_state) return;

	printf("SCSI bus: %02x %c%c%c%c%c%c%c%c%c%c\n",
		state_ & 0xff,
		(state_ & Line::Parity) ? 'p' : '-',
		(state_ & Line::SelectTarget) ? 's' : '-',
		(state_ & Line::Attention) ? 't' : '-',
		(state_ & Line::Control) ? 'c' : '-',
		(state_ & Line::Busy) ? 'b' : '-',
		(state_ & Line::Acknowledge) ? 'a' : '-',
		(state_ & Line::Reset) ? 'r' : '-',
		(state_ & Line::Input) ? 'i' : '-',
		(state_ & Line::Message) ? 'm' : '-',
		(state_ & Line::Request) ? 'q' : '-'
	);

	bool was_asleep = preferred_clocking() == ClockingHint::Preference::None;
	dispatch_index_ = 0;
	time_in_state_ = HalfCycles(0);
	if(was_asleep) update_clocking_observer();
}

BusState Bus::get_state() {
	return state_;
}

void Bus::add_observer(Observer *observer) {
	observers_.push_back(observer);
}

ClockingHint::Preference Bus::preferred_clocking() {
	return (dispatch_index_ < sizeof(dispatch_times_)) ? ClockingHint::Preference::RealTime : ClockingHint::Preference::None;
}

void Bus::run_for(HalfCycles time) {
	if(dispatch_index_ < sizeof(dispatch_times_)) {
		time_in_state_ += time;

		const auto old_index = dispatch_index_;
		const auto time_as_int = time_in_state_.as_int();
		while(time_as_int >= dispatch_times_[dispatch_index_] && dispatch_index_ < sizeof(dispatch_times_)) {
			++dispatch_index_;
		}

		if(dispatch_index_ != old_index) {
			for(auto &observer: observers_) {
				observer->scsi_bus_did_change(this, state_, double(time_as_int) * cycles_to_time_);
			}
		}

		if(preferred_clocking() == ClockingHint::Preference::None) {
			update_clocking_observer();
		}
	}
}
