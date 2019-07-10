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

// MARK: - Control input/output.

void DoubleDensityDrive::set_enabled(bool) {
}

void DoubleDensityDrive::set_control_lines(int lines) {
}

bool DoubleDensityDrive::read() {
	return false;
}

void DoubleDensityDrive::write(bool value) {
}

// MARK: - Speed Selection

void DoubleDensityDrive::did_step(Storage::Disk::HeadPosition to_position) {
	// The 800kb drive automatically selects rotation speed as a function of
	// head position; the 400kb drive doesn't do so.
	if(is_800k_) {
		/*
			Numbers below cribbed from the Kryoflux forums.
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
