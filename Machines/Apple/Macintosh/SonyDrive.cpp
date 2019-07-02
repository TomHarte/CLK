//
//  SonyDrive.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "SonyDrive.hpp"

using namespace Apple::Macintosh;

SonyDrive::SonyDrive(int input_clock_rate, bool is_800k) :
	Storage::Disk::Drive(static_cast<unsigned int>(input_clock_rate), is_800k ? 2 : 1), is_800k_(is_800k) {
	// Start with a valid rotation speed.
	if(is_800k) {
		set_rotation_speed(393.3807f);
	}
}

void SonyDrive::did_step(Storage::Disk::HeadPosition to_position) {
	// The 800kb drive automatically selects rotation speed as a function of
	// head position; the 400kb drive doesn't do so.
	if(is_800k_) {
		/*
			Numbers below cribbed from the Kryoflux forums.
		*/
//		printf("Head moved to %d\n", to_position.as_int());
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
