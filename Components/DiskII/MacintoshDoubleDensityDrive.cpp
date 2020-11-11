//
//  MacintoshDoubleDensityDrive.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/07/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "MacintoshDoubleDensityDrive.hpp"

/*
	Sources used pervasively:

	http://members.iinet.net.au/~kalandi/apple/AUG/1991/11%20NOV.DEC/DISK.STUFF.html
	Apple Guide to the Macintosh Family Hardware
	Inside Macintosh III
*/

using namespace Apple::Macintosh;

DoubleDensityDrive::DoubleDensityDrive(int input_clock_rate, bool is_800k) :
	IWMDrive(input_clock_rate, is_800k ? 2 : 1),	// Only 800kb drives are double sided.
	is_800k_(is_800k) {
	// Start with a valid rotation speed.
	if(is_800k) {
		Drive::set_rotation_speed(393.3807f);
	}
}

// MARK: - Speed Selection

void DoubleDensityDrive::did_step(Storage::Disk::HeadPosition to_position) {
//	printf("At track %d\n", to_position.as_int());
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
			case 0:		Drive::set_rotation_speed(393.3807f);	break;
			case 1:		Drive::set_rotation_speed(429.1723f);	break;
			case 2:		Drive::set_rotation_speed(472.1435f);	break;
			case 3:		Drive::set_rotation_speed(524.5672f);	break;
			default:	Drive::set_rotation_speed(590.1098f);	break;
		}
	}
}

void DoubleDensityDrive::set_rotation_speed(float revolutions_per_minute) {
	if(!is_800k_) {
		// Don't allow drive speeds to drop below 10 RPM, as a temporary sop
		// to sanity.
		Drive::set_rotation_speed(std::max(10.0f, revolutions_per_minute));
	}
}

// MARK: - Control input/output.

void DoubleDensityDrive::set_enabled(bool enabled) {
	// Disabling a drive also stops its motor.
	if(!enabled) set_motor_on(false);
}

void DoubleDensityDrive::set_control_lines(int lines) {
	const auto old_state = control_state_;
	control_state_ = lines;

	// Catch low-to-high LSTRB transitions.
	if((old_state ^ control_state_) & control_state_ & Line::LSTRB) {
		switch(control_state_ & (Line::CA2 | Line::CA1 | Line::CA0 | Line::SEL)) {
			default:
			break;

			case 0:						// Set step direction — CA2 set => step outward.
			case Line::CA2:
				step_direction_ = (control_state_ & Line::CA2) ? -1 : 1;
			break;

			case Line::CA1:				// Set drive motor — CA2 set => motor off.
			case Line::CA1|Line::CA2:
				set_motor_on(!(control_state_ & Line::CA2));
			break;

			case Line::CA0:				// Initiate a step.
				step(Storage::Disk::HeadPosition(step_direction_));
			break;

			case Line::SEL|Line::CA2:	// Reset new disk flag.
				has_new_disk_ = false;
			break;

			case Line::CA2 | Line::CA1 | Line::CA0:	// Eject the disk.
				set_disk(nullptr);
			break;
		}
	}
}

bool DoubleDensityDrive::read() {
	switch(control_state_ & (CA2 | CA1 | CA0 | SEL)) {
		default:
		return false;

		case 0:					// Head step direction.
								// (0 = inward)
		return step_direction_ <= 0;

		case SEL:				// Disk in place.
								// (0 = disk present)
		return !has_disk();

		case CA0:				// Disk head step completed.
								// (0 = still stepping)
		return true;	// TODO: stepping delay. But at the main Drive level.

		case CA0|SEL:			// Disk locked.
								// (0 = write protected)
		return !get_is_read_only();

		case CA1:				// Disk motor running.
								// (0 = motor on)
		return !get_motor_on();

		case CA1|SEL:			// Head at track 0.
								// (0 = at track 0)
								// "This bit becomes valid beginning 12 msec after the step that places the head at track 0."
		return !get_is_track_zero();

		case CA1|CA0:			// Disk has been ejected.
								// (0 = user has ejected disk)
		return !has_new_disk_;

		case CA1|CA0|SEL:		// Tachometer.
								// (arbitrary)
		return get_tachometer();

		case CA2:				// Read data, lower head.
			set_head(0);
		return false;

		case CA2|SEL:			// Read data, upper head.
			set_head(1);
		return false;

		case CA2|CA1:			// Single- or double-sided drive.
								// (0 = single sided)
		return get_head_count() != 1;

		case CA2|CA1|CA0:		// "Present/HD" (per the Mac Plus ROM)
								// (0 = ??HD??)
								//
								// Alternative explanation: "Disk ready for reading?"
								// (0 = ready)
		return false;

		case CA2|CA1|CA0|SEL:	// Drive installed.
								// (0 = present, 1 = missing)
		return false;
	}
}

void DoubleDensityDrive::did_set_disk() {
	has_new_disk_ = true;
}
