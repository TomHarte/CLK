//
//  FDC.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 22/03/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#pragma once

#include "Components/8272/i8272.hpp"

namespace Amstrad {

/*!
	Wraps the 8272 so as to provide proper clocking and RPM counts, and just directly
	exposes motor control, applying the same value to all drives.
*/
class FDC: public Intel::i8272::i8272 {
private:
	Intel::i8272::BusHandler bus_handler_;

public:
	FDC(const Cycles clock_rate = Cycles(8000000)) :
		i8272(bus_handler_, clock_rate)
	{
		emplace_drive(clock_rate.as<int>(), 300, 1);
		set_drive(1);
	}

	void set_motor_on(const bool on) {
		get_drive().set_motor_on(on);
	}

	void select_drive(int) {
		// TODO: support more than one drive. (and in set_disk)
	}

	void set_disk(std::shared_ptr<Storage::Disk::Disk> disk, int) {
		get_drive().set_disk(disk);
	}

	const Storage::Disk::Disk *disk() const {
		return get_drive().disk();
	}

	void set_activity_observer(Activity::Observer *const observer) {
		get_drive().set_activity_observer(observer, "Drive 1", true);
	}
};

}
