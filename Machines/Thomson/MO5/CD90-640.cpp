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
	// 325 is a peculiar RPM, but seems to match a spin-up test in the disk ROM that polls the WD1770's status register
	// index hole bit and counts time. Furthermore there are other machines with unusual RPMs. Could definitely
	// still just imply an issue elsewhere in the emulator though.
	//
	// TODO: I think the 325 proves some sort of timing issue elsewhere. Should be 360. Investigate.
	emplace_drives(2, 8'000'000, 325, 2);
}

uint8_t CD90_640::control() {
	return control_;	// Possibly only b7 is loaded?
}

void CD90_640::set_control(const uint8_t value) {
	control_ = value;

	// Following along from the schematic in https://github.com/OlivierP-To8/CD90-640/ :
	//
	//	The 74LS123 on the second page is fed with:
	//
	//		D0 = external data line 7
	//		D1 = line 0
	//		D2 = line 1
	//		D3 = line 2
	//
	//	It then routes:
	//
	//		Q0 = DDEN select
	//		Q1 = side sleect
	//		Q2, Q3 = drive selects
	//
	set_is_double_density(!(value & 0x80));
	for_all_drives( [&](Storage::Disk::Drive &drive, size_t) {
		drive.set_head(value & 1);
	});
	set_drive((value >> 1) & 3);
}

void CD90_640::set_motor_on(const bool motor) {
	for_all_drives( [&](Storage::Disk::Drive &drive, size_t) {
		drive.set_motor_on(motor);
	});
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
