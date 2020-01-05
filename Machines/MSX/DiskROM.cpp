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
	drives_[0] = std::make_shared<Storage::Disk::Drive>(8000000, 300, 2);
	drives_[1] = std::make_shared<Storage::Disk::Drive>(8000000, 300, 2);
	set_is_double_density(true);
}

void DiskROM::write(uint16_t address, uint8_t value, bool pc_is_outside_bios) {
	switch(address) {
		case 0x7ff8: case 0x7ff9: case 0x7ffa: case 0x7ffb:
			WD::WD1770::write(address, value);
		break;
		case 0x7ffc:
			selected_head_ = value & 1;
			drives_[0]->set_head(selected_head_);
			drives_[1]->set_head(selected_head_);
		break;
		case 0x7ffd: {
			selected_drive_ = value & 1;
			set_drive(drives_[selected_drive_]);

			bool drive_motor = !!(value & 0x80);
			drives_[0]->set_motor_on(drive_motor);
			drives_[1]->set_motor_on(drive_motor);
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
	WD::WD1770::run_for(Cycles(static_cast<int>(controller_cycles_ / 715909)));
	controller_cycles_ %= 715909;
}

void DiskROM::set_disk(std::shared_ptr<Storage::Disk::Disk> disk, size_t drive) {
	drives_[drive]->set_disk(disk);
}

void DiskROM::set_head_load_request(bool head_load) {
	// Magic!
	set_head_loaded(head_load);
}

void DiskROM::set_activity_observer(Activity::Observer *observer) {
	size_t c = 1;
	for(auto &drive: drives_) {
		drive->set_activity_observer(observer, "Drive " + std::to_string(c), true);
		++c;
	}
}
