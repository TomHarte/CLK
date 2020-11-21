//
//  DiskIIDrive.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/11/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#include "DiskIIDrive.hpp"

using namespace Apple::Disk;

DiskIIDrive::DiskIIDrive(int input_clock_rate) :
	IWMDrive(input_clock_rate, 1) {
	Drive::set_rotation_speed(300.0f);
}

void DiskIIDrive::set_enabled(bool enabled) {
	set_motor_on(enabled);
}

void DiskIIDrive::set_control_lines(int lines) {
	// If the stepper magnet selections have changed, and any is on, see how
	// that moves the head.
	if(lines ^ stepper_mask_ && lines) {
		// Convert from a representation of bits set to the centre of pull.
		int direction = 0;
		if(lines&1) direction += (((stepper_position_ - 0) + 4)&7) - 4;
		if(lines&2) direction += (((stepper_position_ - 2) + 4)&7) - 4;
		if(lines&4) direction += (((stepper_position_ - 4) + 4)&7) - 4;
		if(lines&8) direction += (((stepper_position_ - 6) + 4)&7) - 4;
		const int bits_set = (lines&1) + ((lines >> 1)&1) + ((lines >> 2)&1) + ((lines >> 3)&1);
		direction /= bits_set;

		// Compare to the stepper position to decide whether that pulls in the current cog notch,
		// or grabs a later one.
		step(Storage::Disk::HeadPosition(-direction, 4));
		stepper_position_ = (stepper_position_ - direction + 8) & 7;
		printf("Step %0.2f\n", float(-direction) / 4.0f);
	}
	stepper_mask_ = lines;
}

bool DiskIIDrive::read() {
	return !!(stepper_mask_ & 2) || get_is_read_only();
}
