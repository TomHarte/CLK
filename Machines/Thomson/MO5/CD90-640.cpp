//
//  CD90-640.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/04/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include "CD90-640.hpp"

using namespace Thomson;

CD90_640::CD90_640() : WD::WD1770(P1793) {
	emplace_drives(2, 8000000, 300, 2);
}

uint8_t CD90_640::control() {
	Logger::info().append("Read control: %02x", control_);
	return control_;
}

void CD90_640::set_control(const uint8_t value) {
	Logger::info().append("Set control to %02x", value);
	control_ = value;
	// TODO: What do these bits mean?
}

void CD90_640::set_activity_observer(Activity::Observer *const observer) {
	for_all_drives([observer] (Storage::Disk::Drive &drive, size_t index) {
		drive.set_activity_observer(observer, "Drive " + std::to_string(index+1), true);
	});
}

// TODO: the code below is fairly boilerplate; can it be factored out?

void CD90_640::set_disk(std::shared_ptr<Storage::Disk::Disk> disk, const size_t drive) {
	get_drive(drive).set_disk(disk);
}

const Storage::Disk::Disk *CD90_640::disk(const std::string &name) {
	const Storage::Disk::Disk *result = nullptr;
	for_all_drives( [&](Storage::Disk::Drive &drive, size_t) {
		const auto disk = drive.disk();
		if(disk && disk->represents(name)) {
			result = disk;
		}
	});
	return result;
}
