//
//  DiskController.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/05/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include "DiskController.hpp"

using namespace Tandy::CoCo;

DiskController::DiskController() : WD::WD1770(P1773) {
	emplace_drives(4, 8'000'000, 300, 2);
}

void DiskController::set_control(const uint8_t value) {
	// TODO:
	//
	//	b7: halt flag, 1 = enabled
	//	b6: drive select 3
	//	b5: density, 1 = double
	//	b4: write precompensation, 1 = enable
	//	b3: drive motors, 1 = on
	//	b0–b2: drive selects

	double_density_ = value & 0x20;
	set_is_double_density(value & 0x20);
	for_all_drives([&] (Storage::Disk::Drive &drive, size_t) {
		drive.set_motor_on(value & 0x8);
	});
	set_drive((value & 7) | ((value >> 3) & 8));
	enable_halt_ = value & 0x80;
}

void DiskController::set_disk(std::shared_ptr<Storage::Disk::Disk> disk, const size_t drive) {
	get_drive(drive).set_disk(disk);
}

void DiskController::set_activity_observer(Activity::Observer *const observer) {
	for_all_drives([observer] (Storage::Disk::Drive &drive, size_t index) {
		drive.set_activity_observer(observer, "Drive " + std::to_string(index+1), true);
	});
}

const Storage::Disk::Disk *DiskController::disk(const std::string &name) {
	const Storage::Disk::Disk *result = nullptr;
	for_all_drives( [&](Storage::Disk::Drive &drive, size_t) {
		const auto disk = drive.disk();
		if(disk && disk->represents(name)) {
			result = disk;
		}
	});
	return result;
}

//
// TODO:
//
//	* not INTRQ hits a NOR gate with DDEN; NOR of that is piped to NMI; and
//	* DRQ informs HALT, if enabled.
//

bool DiskController::halt() const {
	return get_data_request_line() && enable_halt_;
}

bool DiskController::nmi() const {
	return get_interrupt_request_line() && double_density_;
}
