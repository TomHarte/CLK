//
//  MacintoshDoubleDensityDrive.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/07/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "MacintoshDoubleDensityDrive.hpp"

using namespace Apple::Macintosh;

DoubleDensityDrive::DoubleDensityDrive(int input_clock_rate, bool is_800k) :
	IWMDrive(input_clock_rate, is_800k ? 2 : 1),	// Only 800kb drives are double sided.
	is_800k_(is_800k) {
	// Start with a valid rotation speed.
	if(is_800k) {
		set_rotation_speed(393.3807f);
	}
}

// MARK: - Speed Selection

void DoubleDensityDrive::did_step(Storage::Disk::HeadPosition to_position) {
	// The 800kb drive automatically selects rotation speed as a function of
	// head position; the 400kb drive doesn't do so.
	if(is_800k_) {
		/*
			Numbers below cribbed from the Kryoflux forums; specifically:
			https://forum.kryoflux.com/viewtopic.php?t=1090

			They can almost be worked out algorithmically, since the point is to
			produce an almost-constant value for speed*(number of sectors), and:

			393.3807 * 12 = 4720.5684
			429.1723 * 11 = 4720.895421
			472.1435 * 10 = 4721.435
			524.5672 * 9 = 4721.1048
			590.1098 * 8 = 4720.8784

			So 4721 / (number of sectors per track in zone) would give essentially
			the same results.
		*/
		const int zone = to_position.as_int() >> 4;
		switch(zone) {
			case 0:		set_rotation_speed(393.3807f);	break;
			case 1:		set_rotation_speed(429.1723f);	break;
			case 2:		set_rotation_speed(472.1435f);	break;
			case 3:		set_rotation_speed(524.5672f);	break;
			default:	set_rotation_speed(590.1098f);	break;
		}
	}
}

// MARK: - Control input/output.

void DoubleDensityDrive::set_enabled(bool) {
}

void DoubleDensityDrive::set_control_lines(int lines) {
	const auto old_state = control_state_;
	control_state_ = lines;

	// Catch low-to-high LSTRB transitions.
	if((old_state ^ control_state_) & control_state_ & Line::LSTRB) {
		switch(control_state_ & (Line::CA1 | Line::CA0 | Line::SEL)) {
			default:
//				LOG("Unhandled LSTRB");
			break;

			case 0:
//				LOG("LSTRB Set stepping direction: " << int(state_ & CA2));
				step_direction_ = (control_state_ & Line::CA2) ? -1 : 1;
			break;

			case Line::CA1:
//				LOG("LSTRB Motor");
				set_motor_on(!(control_state_ & Line::CA2));
			break;

			case Line::CA0:
//				LOG("LSTRB Step");
				step(Storage::Disk::HeadPosition(step_direction_));
			break;

			case Line::CA1 | Line::CA0:
//				LOG("LSTRB Eject disk");
				set_disk(nullptr);
			break;
		}
	}
}

bool DoubleDensityDrive::read() {
	switch(control_state_ & (CA2 | CA1 | CA0 | SEL)) {
		default:
//			LOG("unknown)");
		return false;

		// Possible other meanings:
		// B = ready			(0 = ready)
		//
		// {CA1,CA0,SEL,CA2}

		case 0:					// Head step direction.
								// (0 = inward)
//			LOG("head step direction)");
		return step_direction_ <= 0;

		case SEL:				// Disk in place.
								// (0 = disk present)
//			LOG("disk in place)");
		return !has_disk();

		case CA0:				// Disk head step completed.
								// (0 = still stepping)
//			LOG("head stepping)");
		return true;

		case CA0|SEL:			// Disk locked.
								// (0 = write protected)
//			LOG("disk locked)");
		return !get_is_read_only();

		case CA1:				// Disk motor running.
								// (0 = motor on)
//			LOG("disk motor running)");
		return !get_motor_on();

		case CA1|SEL:			// Head at track 0.
								// (0 = at track 0)
//			LOG("head at track 0)");
		return !get_is_track_zero();

		case CA1|CA0|SEL:		// Tachometer.
								// (arbitrary)
		return get_tachometer();

		case CA2:				// Read data, lower head.
//			LOG("data, lower head)\n");
			set_head(0);
		return false;;

		case CA2|SEL:			// Read data, upper head.
//			LOG("data, upper head)\n");
			set_head(1);
		return false;

		case CA2|CA1:			// Single- or double-sided drive.
								// (0 = single sided)
//			LOG("single- or double-sided drive)");
		return get_head_count() != 1;

		case CA2|CA1|CA0:		// "Present/HD" (per the Mac Plus ROM)
								// (0 = ??HD??)
//			LOG("present/HD)");
		return false;

		case CA2|CA1|CA0|SEL:	// Drive installed.
								// (0 = present, 1 = missing)
//			LOG("drive installed)");
		return false;
	}
}

void DoubleDensityDrive::write(bool value) {
}
