//
//  ReactiveDevice.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 12/02/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "ReactiveDevice.hpp"

using namespace Apple::ADB;

ReactiveDevice::ReactiveDevice(Apple::ADB::Bus &bus) : device_id_(bus.add_device(this)) {}

void ReactiveDevice::post_response(const std::vector<uint8_t> &&response) {
	response_ = std::move(response);
}

void ReactiveDevice::advance_state(double microseconds) {
	(void)microseconds;
}
