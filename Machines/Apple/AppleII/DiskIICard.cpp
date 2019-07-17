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
	const auto roms = rom_fetcher(
		"DiskII",
		{
			is_16_sector ? "boot-16.rom" : "boot-13.rom",
			is_16_sector ? "state-machine-16.rom" : "state-machine-13.rom"
		});
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
					*value = static_cast<uint8_t>(disk_value);
			}
		} break;
		case Device:
			if(is_read) *value = boot_[address & 0xff];
		break;
	}
}

void DiskIICard::run_for(Cycles cycles, int stretches) {
	if(diskii_clocking_preference_ == ClockingHint::Preference::None) return;
	diskii_.run_for(Cycles(cycles.as_int() * 2));
}

void DiskIICard::set_disk(const std::shared_ptr<Storage::Disk::Disk> &disk, int drive) {
	diskii_.set_disk(disk, drive);
}

void DiskIICard::set_activity_observer(Activity::Observer *observer) {
	diskii_.set_activity_observer(observer);
}

void DiskIICard::set_component_prefers_clocking(ClockingHint::Source *component, ClockingHint::Preference preference) {
	diskii_clocking_preference_ = preference;
	set_select_constraints((preference != ClockingHint::Preference::RealTime) ? (IO | Device) : 0);
}

Storage::Disk::Drive &DiskIICard::get_drive(int drive) {
	return diskii_.get_drive(drive);
}
