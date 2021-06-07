//
//  DiskII.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/04/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#include "DiskIICard.hpp"

using namespace Apple::II;

ROM::Request DiskIICard::rom_request(bool is_16_sector) {
	if(is_16_sector) {
		return ROM::Request(ROM::Name::DiskIIBoot16Sector) && ROM::Request(ROM::Name::DiskIIStateMachine16Sector);
	} else {
		/* TODO: once the DiskII knows how to decode common images of the 13-sector state machine, use that instead of the 16-sector. */
		return ROM::Request(ROM::Name::DiskIIBoot13Sector) && ROM::Request(ROM::Name::DiskIIStateMachine16Sector);
	}
}


DiskIICard::DiskIICard(ROM::Map &map, bool is_16_sector) : diskii_(2045454) {
	std::vector<std::unique_ptr<std::vector<uint8_t>>> roms;
	ROM::Map::iterator state_machine, boot;
	if(is_16_sector) {
		state_machine = map.find(ROM::Name::DiskIIStateMachine16Sector);
		boot = map.find(ROM::Name::DiskIIBoot16Sector);
	} else {
		// TODO: see above re: 13-sector state machine.
		state_machine = map.find(ROM::Name::DiskIIStateMachine16Sector);
		boot = map.find(ROM::Name::DiskIIBoot13Sector);
	}

	if(state_machine == map.end() || boot == map.end()) {
		throw ROMMachine::Error::MissingROMs;
	}

	boot_ = std::move(boot->second);
	diskii_.set_state_machine(state_machine->second);
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
