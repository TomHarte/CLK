//
//  BD500.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/01/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#include "BD500.hpp"

using namespace Oric;

BD500::BD500() : DiskController(P1793, 9000000, Storage::Disk::Drive::ReadyType::ShugartModifiedRDY) {
	disable_basic_rom_ = true;
	select_paged_item();
	set_is_double_density(true);
}

void BD500::write(int address, uint8_t value) {
	access(address);

	if(address >= 0x0320 && address <= 0x0323) {
//		if(address == 0x320) printf("Command %02x\n", value);
		WD::WD1770::write(address, value);
	}
}

uint8_t BD500::read(int address) {
	access(address);

	switch(address) {
		default: return 0xff;

		case 0x0320: case 0x0321: case 0x0322: case 0x0323:
		return WD::WD1770::read(address);

		case 0x312:	return (get_data_request_line() ? 0x80 : 0x00) | (get_interrupt_request_line() ? 0x40 : 0x00);
	}
}

void BD500::access(int address) {
	// Determine whether to perform a command.
	switch(address) {
		case 0x0320: case 0x0321: case 0x0322: case 0x0323: case 0x0312:
		return;

		case 0x310:	enable_overlay_ram_ = true;		break;
		case 0x313:	enable_overlay_ram_ = false;	break;
		case 0x317:	disable_basic_rom_ = false;		break;		// Could be 0x311.

		default:
//			printf("Switch %04x???\n", address);
		break;
	}

	select_paged_item();
}

/*
	The following was used when trying to find appropriate soft switch locations. It is preserved
	as the values I have above are unlikely to be wholly correct and further research might be
	desirable.

void BD500::access(int address) {
	//	0,1,4,5,10,11 -> 64kb Atmos
	//	2,3,9 -> 56kb Atmos.
	// Broken: 6, 7, 8

	int order = 5;
	int commands[4];
	std::vector<int> available_commands = {0, 1, 2, 3};
	const int modulos[] = {6, 2, 1, 1};

	for(int c = 0; c < 4; ++c) {
		const int index = order / modulos[c];
		commands[c] = available_commands[size_t(index)];
		available_commands.erase(available_commands.begin() + index);
		order %= modulos[c];
	}


	// Determine whether to perform a command.
	int index = -1;
	switch(address) {
		case 0x0320: case 0x0321: case 0x0322: case 0x0323: case 0x0312:
		return;

		case 0x310: index = 0; break;
		case 0x313: index = 1; break;
		case 0x314: index = 2; break;
		case 0x317: index = 3; break;

		default:
			printf("Switch %04x???\n", address);
		break;
	}

	select_paged_item();

	if(index >= 0) {
		switch(commands[index]) {
			case 0:	enable_overlay_ram_ = true;		break;		// +RAM
			case 1: disable_basic_rom_ = false;		break;		// -rom
			case 2: disable_basic_rom_ = true;		break;		// +rom
			case 3:	enable_overlay_ram_ = false;	break;		// -RAM

		}
		select_paged_item();
	}
}
*/

void BD500::set_head_load_request(bool head_load) {
	// Turn all motors on or off; if off then unload the head instantly.
	is_loading_head_ |= head_load;
	for(auto &drive : drives_) {
		if(drive) drive->set_motor_on(head_load);
	}
	if(!head_load) set_head_loaded(false);
}

void BD500::run_for(const Cycles cycles) {
	// If a head load is in progress and the selected drive is now ready,
	// declare head loaded.
	if(is_loading_head_ && drives_[selected_drive_] && drives_[selected_drive_]->get_is_ready()) {
		set_head_loaded(true);
		is_loading_head_ = false;
	}

	WD::WD1770::run_for(cycles);
}

void BD500::set_activity_observer(Activity::Observer *observer) {
	observer_ = observer;
	if(observer) {
		observer->register_led("BD-500");
		observer_->set_led_status("BD-500", get_head_loaded());
	}
}

void BD500::set_head_loaded(bool loaded) {
	WD::WD1770::set_head_loaded(loaded);
	if(observer_) {
		observer_->set_led_status("BD-500", loaded);
	}
}
