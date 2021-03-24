//
//  BD500.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/01/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#include "BD500.hpp"

using namespace Oric;

/*
	Notes on the code below: the Byte Drive 500 isn't well documented; this implementation is based on
	experimentation without access to real hardware as documented in http://forum.defence-force.org/viewtopic.php?f=25&t=2055
	and with additional guidance from iss of Oricutron (amongst other things) elsewhere,
	e.g. http://forum.defence-force.org/viewtopic.php?f=22&t=1698&start=105#p21468
*/

BD500::BD500() : DiskController(P1793, 9000000, Storage::Disk::Drive::ReadyType::ShugartModifiedRDY) {
	disable_basic_rom_ = true;
	select_paged_item();
	set_is_double_density(true);
	set_drive(1);
}

void BD500::write(int address, uint8_t value) {
	access(address);

	if(address >= 0x0320 && address <= 0x0323) {
//		if(address == 0x320) printf("Command %02x\n", value);
		WD::WD1770::write(address, value);
	}

	if(address == 0x031a) {
		// Drive select; kudos to iss of Oricutron for figuring this one out;
		// cf. http://forum.defence-force.org/viewtopic.php?f=25&p=21409#p21393
		switch(value & 0xe0) {
			default:	set_drive(0);	break;
			case 0x20:	set_drive(1);	break;
			case 0x40:	set_drive(2);	break;
			case 0x80:	set_drive(4);	break;
			case 0xc0:	set_drive(8);	break;
		}
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

		case 0x311:	disable_basic_rom_ = true;		break;
		case 0x313:	enable_overlay_ram_ = false;	break;
		case 0x314:	enable_overlay_ram_ = true;		break;
		case 0x317:	disable_basic_rom_ = false;		break;

		default:
//			printf("Switch %04x???\n", address);
		break;
	}

	select_paged_item();
}

void BD500::set_head_load_request(bool head_load) {
	// Turn all motors on or off; if off then unload the head instantly.
	is_loading_head_ |= head_load;
	for_all_drives([head_load] (Storage::Disk::Drive &drive, size_t) {
		drive.set_motor_on(head_load);
	});
	if(!head_load) set_head_loaded(false);
}

void BD500::run_for(const Cycles cycles) {
	// If a head load is in progress and the selected drive is now ready,
	// declare head loaded.
	if(is_loading_head_ && get_drive().get_is_ready()) {
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
