//
//  CD90-640.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/04/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include "CD90-640.hpp"

using namespace Thomson;

CD90_640::CD90_640() : WD::WD1770(P1770) {
	emplace_drives(2, 8000000, 300, 2);
}

uint8_t CD90_640::control() {
	return control_;
}

void CD90_640::set_control(const uint8_t value) {
	control_ = value;
	// TODO: What do these bits mean?
}

void CD90_640::set_activity_observer(Activity::Observer *const observer) {
	for_all_drives([observer] (Storage::Disk::Drive &drive, size_t index) {
		drive.set_activity_observer(observer, "Drive " + std::to_string(index+1), true);
	});
}
