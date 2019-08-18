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
	printf("%08x output\n", output);
	device_states_[device] = output;
	state_is_valid_ = false;
}

BusState Bus::get_state() {
	if(!state_is_valid_) return state_;

	state_is_valid_ = true;
	state_ = DefaultBusState;
	for(auto state: device_states_) {
		state_ |= state;
	}

	return state_;
}
