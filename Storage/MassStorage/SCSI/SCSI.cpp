//
//  SCSI.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 12/08/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "SCSI.hpp"

using namespace SCSI;

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
		(state_ & Line::Attention) ? 'a' : '-',
		(state_ & Line::Control) ? 'c' : '-',
		(state_ & Line::Busy) ? 'b' : '-',
		(state_ & Line::Acknowledge) ? 'a' : '-',
		(state_ & Line::Reset) ? 'r' : '-',
		(state_ & Line::Input) ? 'i' : '-',
		(state_ & Line::Message) ? 'm' : '-',
		(state_ & Line::Request) ? 'q' : '-'
	);

	for(auto &observer: observers_) {
		observer->scsi_bus_did_change(this, state_);
	}
}

BusState Bus::get_state() {
	return state_;
}

void Bus::add_observer(Observer *observer) {
	observers_.push_back(observer);
}
