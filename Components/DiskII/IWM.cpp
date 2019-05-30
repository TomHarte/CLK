//
//  IWM.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/05/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "IWM.hpp"

#include <cstdio>

using namespace Apple;

IWM::IWM(int clock_rate) {

}

uint8_t IWM::read(int address) {
	access(address);

	printf("IWM r %d (%02x)\n", address&0xf, q_switches_);
	switch(q_switches_) {
		default:	return 0x00;	// Undefined.

		case 0x20:	return 0x00;	// Data register.

		case 0x40:
		case 0x60:
		return (mode_&0x1f);		// Status register.

		case 0x80:
		case 0xa0:
		return 0x80;				// Handshake register.
	}
}

void IWM::write(int address, uint8_t input) {
	access(address);

	printf("IWM w %d (%02x)\n", address&0xf);
	switch(q_switches_) {
		default:
		break;

		case 0xc0:	// Write mode register.
			mode_ = input;
		break;

		case 0xd0:	// Write data register.
		break;

	}
}

void IWM::access(int address) {
	switch(address & 0xf) {
		default:
		break;

		case 0x8:	q_switches_ &= ~0x20;			break;
		case 0x9:	q_switches_ |= 0x20;			break;
		case 0xc:	q_switches_ &= ~0x40;			break;
		case 0xd:	q_switches_ |= 0x40;			break;
		case 0xe:	q_switches_ &= ~0x80;			break;
		case 0xf:	q_switches_ |= 0x80;			break;
	}
}

void IWM::run_for(const Cycles cycles) {
}

void IWM::set_select(bool enabled) {
	printf("IWM s %d\n", int(enabled));
}
