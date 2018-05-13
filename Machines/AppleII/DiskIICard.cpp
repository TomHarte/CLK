//
//  DiskII.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/04/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#include "DiskIICard.hpp"

using namespace AppleII;

DiskIICard::DiskIICard(const ROMMachine::ROMFetcher &rom_fetcher, bool is_16_sector) {
	auto roms = rom_fetcher(
		"DiskII",
		{
			is_16_sector ? "boot-16.rom" : "boot-13.rom",
			is_16_sector ? "state-machine-16.rom" : "state-machine-13.rom"
		});
	boot_ = std::move(*roms[0]);
	diskii_.set_state_machine(*roms[1]);
}

void DiskIICard::perform_bus_operation(CPU::MOS6502::BusOperation operation, uint16_t address, uint8_t *value) {
	if(address < 0x100) {
		if(isReadOperation(operation)) *value &= boot_[address];
	} else {
		if(isReadOperation(operation)) {
			*value = diskii_.get_register(address);
		} else {
			diskii_.set_register(address, *value);
		}
	}
}

void DiskIICard::run_for(Cycles cycles, int stretches) {
	diskii_.run_for(Cycles(cycles.as_int() * 2));
}

void DiskIICard::set_disk(const std::shared_ptr<Storage::Disk::Disk> &disk, int drive) {
	diskii_.set_disk(disk, drive);
}

void DiskIICard::set_activity_observer(Activity::Observer *observer) {
	diskii_.set_activity_observer(observer);
}
