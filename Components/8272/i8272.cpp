//
//  i8272.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/08/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "i8272.hpp"

#include <cstdio>

using namespace Intel;

i8272::i8272(Cycles clock_rate, int clock_rate_multiplier, int revolutions_per_minute) :
	Storage::Disk::MFMController(clock_rate, clock_rate_multiplier, revolutions_per_minute) {
}

void i8272::run_for(Cycles cycles) {
	Storage::Disk::MFMController::run_for(cycles);
}

void i8272::set_register(int address, uint8_t value) {
	if(!address) return;

	// TODO: if not accepting commands, return

	command_.push_back(value);
	size_t necessary_length;
	switch(command_[0] & 0x1f) {
		case 0x06:	// read data
			necessary_length = 9;
		break;
		case 0x0b:	// read deleted data
			necessary_length = 9;
		break;
		case 0x05:	// write data
			necessary_length = 9;
		break;
		case 0x09:	// write deleted data
			necessary_length = 9;
		break;
		case 0x02:	// read track
			necessary_length = 9;
		break;
		case 0x0a:	// read ID
			necessary_length = 2;
		break;
		case 0x0d:	// format track
			necessary_length = 6;
		break;
		case 0x11:	// scan low
			necessary_length = 9;
		break;
		case 0x19:	// scan low  or equal
			necessary_length = 9;
		break;
		case 0x1d:	// scan high or equal
			necessary_length = 9;
		break;
		case 0x07:	// recalibrate
			necessary_length = 2;
		break;
		case 0x08:	// sense interrupt status
			necessary_length = 1;
		break;
		case 0x03:	// specify
			necessary_length = 3;
		break;
		case 0x04:	// sense drive status
			necessary_length = 2;
		break;
		case 0x0f:	// seek
			necessary_length = 3;
		break;
		default:	// invalid
			necessary_length = 1;
		break;
	}

	printf("8272 set register %d to %02x\n", address, value);

	if(command_.size() == necessary_length) {
		printf("8272 Perform!\n");
		command_.clear();
	}
}

uint8_t i8272::get_register(int address) {
	if(address) {
		printf("8272 get data\n");
		return 0xff;
	} else {
		printf("8272 get status\n");
		return 0x80;
	}
}

void i8272::posit_event(Event type) {
}
