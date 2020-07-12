//
//  DiskROM.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/01/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#include "DiskROM.hpp"

using namespace MSX;

DiskROM::DiskROM(const std::vector<uint8_t> &rom) :
	WD1770(P1793),
	rom_(rom) {
	emplace_drives(2, 8000000, 300, 2);
	set_is_double_density(true);
}

void DiskROM::write(uint16_t address, uint8_t value, bool) {
	switch(address) {
		case 0x7ff8: case 0x7ff9: case 0x7ffa: case 0x7ffb:
			WD::WD1770::write(address, value);
		break;
		case 0x7ffc: {
			const int selected_head = value & 1;
			for_all_drives([selected_head] (Storage::Disk::Drive &drive, size_t) {
				drive.set_head(selected_head);
			});
		} break;
		case 0x7ffd: {
			set_drive(1 << (value & 1));

			const bool drive_motor = value & 0x80;
			for_all_drives([drive_motor] (Storage::Disk::Drive &drive, size_t) {
				drive.set_motor_on(drive_motor);
			});
		} break;
	}
}

uint8_t DiskROM::read(uint16_t address) {
	if(address >= 0x7ff8 && address < 0x7ffc) {
		return WD::WD1770::read(address);
	}
	if(address == 0x7fff) {
		return (get_data_request_line() ? 0x00 : 0x80) | (get_interrupt_request_line() ? 0x00 : 0x40);
	}
	return rom_[address & 0x3fff];
}

void DiskROM::run_for(HalfCycles half_cycles) {
	// Input clock is going to be 7159090/2 Mhz, but the drive controller
	// needs an 8Mhz clock, so scale up. 8000000/7159090 simplifies to
	// 800000/715909.
	controller_cycles_ += 800000 * half_cycles.as_integral();
	WD::WD1770::run_for(Cycles(int(controller_cycles_ / 715909)));
	controller_cycles_ %= 715909;
}

void DiskROM::set_disk(std::shared_ptr<Storage::Disk::Disk> disk, size_t drive) {
	get_drive(drive).set_disk(disk);
}

void DiskROM::set_head_load_request(bool head_load) {
	// Magic!
	set_head_loaded(head_load);
}

void DiskROM::set_activity_observer(Activity::Observer *observer) {
	for_all_drives([observer] (Storage::Disk::Drive &drive, size_t index) {
		drive.set_activity_observer(observer, "Drive " + std::to_string(index), true);
	});
}
