//
//  Mouse.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 12/02/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "Mouse.hpp"

using namespace Apple::ADB;

Mouse::Mouse(Bus &bus) : ReactiveDevice(bus) {}

void Mouse::adb_bus_did_observe_event(Bus::Event event, uint8_t value) {
	if(!next_is_command_ && event != Bus::Event::Attention) {
		return;
	}

	if(next_is_command_ && event == Bus::Event::Byte) {
		next_is_command_ = false;

		const auto command = decode_command(value);
		if(command.device != 3) {
			return;
		}
	} else if(event == Bus::Event::Attention) {
		next_is_command_ = true;
	}
}
