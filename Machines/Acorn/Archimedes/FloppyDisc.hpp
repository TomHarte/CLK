//
//  FloppyDisc.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/04/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#pragma once

#include "../../../Components/1770/1770.hpp"

namespace Archimedes {

template <typename InterruptObserverT>
class FloppyDisc: public WD::WD1770, public WD::WD1770::Delegate {
public:
	FloppyDisc(InterruptObserverT &observer) : WD::WD1770(P1772), observer_(observer) {
		emplace_drives(1, 8000000, 300, 2);
		set_delegate(this);
	}

	void wd1770_did_change_output(WD::WD1770 *) override {
		observer_.update_interrupts();
	}

	void set_control(uint8_t value) {
		//	b0, b1, b2, b3 = drive selects;
		//	b4 = side select;
		//	b5 = motor on/off
		//	b6 = floppy in use (i.e. LED?);
		//	b7 = disc eject/change reset.
		set_drive((value & 0x1) ^ 0x1);
		get_drive().set_head(1 ^ ((value >> 4) & 1));
		get_drive().set_motor_on(!(value & 0x20));
	}
	void reset() {}

	void set_disk(std::shared_ptr<Storage::Disk::Disk> disk, size_t drive) {
		get_drive(drive).set_disk(disk);
	}

private:
	InterruptObserverT &observer_;
};

}
