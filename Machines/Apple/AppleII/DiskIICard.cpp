//
//  DiskII.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/04/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#include "DiskIICard.hpp"

using namespace Apple::II;

DiskIICard::DiskIICard(const ROMMachine::ROMFetcher &rom_fetcher, bool is_16_sector) : diskii_(2045454) {
	std::vector<std::unique_ptr<std::vector<uint8_t>>> roms;
	if(is_16_sector) {
		roms = rom_fetcher({
			{"DiskII", "the Disk II 16-sector boot ROM", "boot-16.rom", 256, 0xce7144f6},
			{"DiskII", "the Disk II 16-sector state machine ROM", "state-machine-16.rom", 256, { 0x9796a238, 0xb72a2c70 } }
		});
	} else {
		roms = rom_fetcher({
			{"DiskII", "the Disk II 13-sector boot ROM", "boot-13.rom", 256, 0xd34eb2ff},
			{"DiskII", "the Disk II 13-sector state machine ROM", "state-machine-13.rom", 256, 0x62e22620 }
		});
	}
	if(!roms[0] || !roms[1]) {
		throw ROMMachine::Error::MissingROMs;
	}

	boot_ = std::move(*roms[0]);
	diskii_.set_state_machine(*roms[1]);
	set_select_constraints(None);
	diskii_.set_clocking_hint_observer(this);
}

void DiskIICard::perform_bus_operation(Select select, bool is_read, uint16_t address, uint8_t *value) {
	diskii_.set_data_input(*value);
	switch(select) {
		default: break;
		case IO: {
			const int disk_value = diskii_.read_address(address);
			if(is_read) {
				if(disk_value != diskii_.DidNotLoad)
					*value = uint8_t(disk_value);
			}
		} break;
		case Device:
			if(is_read) *value = boot_[address & 0xff];
		break;
	}
}

void DiskIICard::run_for(Cycles cycles, int) {
	if(diskii_clocking_preference_ == ClockingHint::Preference::None) return;
	diskii_.run_for(Cycles(cycles.as_integral() * 2));
}

void DiskIICard::set_disk(const std::shared_ptr<Storage::Disk::Disk> &disk, int drive) {
	diskii_.set_disk(disk, drive);
}

void DiskIICard::set_activity_observer(Activity::Observer *observer) {
	diskii_.set_activity_observer(observer);
}

void DiskIICard::set_component_prefers_clocking(ClockingHint::Source *, ClockingHint::Preference preference) {
	diskii_clocking_preference_ = preference;
	set_select_constraints((preference != ClockingHint::Preference::RealTime) ? (IO | Device) : None);
}

Storage::Disk::Drive &DiskIICard::get_drive(int drive) {
	return diskii_.get_drive(drive);
}
