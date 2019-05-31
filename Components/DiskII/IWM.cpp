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

namespace  {
	const int CA0		= 1 << 0;
	const int CA1		= 1 << 1;
	const int CA2		= 1 << 2;
	const int LSTRB		= 1 << 3;
	const int ENABLE	= 1 << 4;
	const int DRIVESEL	= 1 << 5;	/* This means drive select, like on the original Disk II. */
	const int Q6		= 1 << 6;
	const int Q7		= 1 << 7;
	const int SEL		= 1 << 8;	/* This is an additional input, not available on a Disk II, with a confusingly-similar name to SELECT but a distinct purpose. */
}

IWM::IWM(int clock_rate) {}

// MARK: - Bus accessors

uint8_t IWM::read(int address) {
	access(address);

	// Per Inside Macintosh:
	//
	// "Before you can read from any of the disk registers you must set up the state of the IWM so that it
	// can pass the data through to the MC68000's address space where you'll be able to read it. To do that,
	// you must first turn off Q7 by reading or writing dBase+q7L. Then turn on Q6 by accessing dBase+q6H.
	// After that, the IWM will be able to pass data from the disk's RD/SENSE line through to you."
	//
	// My thoughts: I'm unclear on how passing data from RD/SENSE is conflated with reading "any of the disk
	// registers", but the text reads as though the access happens internally but isn't passed on in the case
	// of Q6 and Q7 being in the incorrect state.

	printf("Read %d: ", address&1);

	switch(state_ & (Q6 | Q7 | ENABLE)) {
		default:
			printf("Invalid read\n");
		return 0xff;

		// "Read all 1s".
		case 0:
			printf("Reading all 1s\n");
		return 0xff;

		case ENABLE:				/* Read data register. */
			printf("Reading data register\n");
		return 0x00;

		case Q6: case Q6|ENABLE: {
			/*
				[If A = 0], Read status register:

				bits 0-3: same as mode register.
				bit 5: 1 = either /ENBL1 or /ENBL2 is currently low.
				bit 6: 1 = MZ (reserved for future compatibility; should always be read as 0).
				bit 7: 1 = SENSE input high; 0 = SENSE input low.

				(/ENBL1 is low when the first drive's motor is on; /ENBL2 is low when the second drive's motor is on.
				If the 1-second timer is enabled, motors remain on for one second after being programmatically disabled.)
			*/
			printf("Reading status ([%d] including ", (state_&DRIVESEL) ? 2 : 1);

			// Determine the SENSE input.
			uint8_t sense = 0;
			switch(state_ & (CA2 | CA1 | CA0 | SEL)) {
				default:
					printf("unknown)\n");
				break;

				case 0:					// Head step direction.
					printf("head step direction)\n");
				break;

				case SEL:				// Disk in place.
					printf("disk in place)\n");
					sense = 0x80;
				break;

				case CA0:				// Disk head stepping.
					printf("head stepping)\n");
				break;

				case CA0|SEL:			// Disk locked (i.e. write-protect tab).
					printf("disk locked)\n");
				break;

				case CA1:				// Disk motor running.
					printf("disk motor running)\n");
				break;

				case CA1|SEL:			// Head at track 0.
					printf("head at track 0)\n");
				break;

				case CA1|CA0|SEL:		// Tachometer (?)
					printf("tachometer)\n");
				break;

				case CA2:				// Read data, lower head.
					printf("data, lower head)\n");
				break;

				case CA2|SEL:			// Read data, upper head.
					printf("data, upper head)\n");
				break;

				case CA2|CA1:			// Single- or double-sided drive.
					printf("single- or double-sided drive)\n");
				break;

				case CA2|CA1|CA0|SEL:	// Drive installed.
					printf("drive installed)\n");
				break;
			}

			return (mode_&0x1f) | sense;
		} break;

		case Q7: case Q7|ENABLE:
			/*
				Read write-handshake register:

				bits 0-5: reserved for future use (currently read as 1).
				bit 6: 1 = write state (cleared to 0 if a write underrun occurs).
				bit 7: 1 = write data buffer ready for data.
			*/
			printf("Reading write handshake\n");
		return 0x1f;
	}

	return 0xff;
}

void IWM::write(int address, uint8_t input) {
	access(address);

	switch(state_ & (Q6 | Q7 | ENABLE)) {
		default: break;

		case Q7|Q6:
			/*
				Write mode register:

				bit 0: 1 = latch mode (should be set in asynchronous mode).
				bit 1: 0 = synchronous handshake protocol; 1 = asynchronous.
				bit 2: 0 = 1-second on-board timer enable; 1 = timer disable.
				bit 3: 0 = slow mode; 1 = fast mode.
				bit 4: 0 = 7Mhz; 1 = 8Mhz (7 or 8 mHz clock descriptor).
				bit 5: 1 = test mode; 0 = normal operation.
				bit 6: 1 = MZ-reset.
				bit 7: reserved for future expansion.
			*/
			mode_ = input;
			printf("IWM mode is now %02x\n", mode_);
		break;

		case Q7|Q6|ENABLE:	// Write data register.
			printf("Data register write\n");
		break;
	}
}

// MARK: - Switch access

void IWM::access(int address) {
	// Keep a record of switch state; bits in state_
	// should correlate with the anonymous namespace constants
	// defined at the top of this file — CA0, CA1, etc.
	address &= 0xf;
	const auto mask = 1 << (address >> 1);

	if(address & 1) {
		state_ |= mask;
	} else {
		state_ &= ~mask;
	}
}

void IWM::set_select(bool enabled) {
	// Augment switch state with the value of the SEL line;
	// it's active low, which is implicitly inverted here for
	// consistency in the meaning of state_ bits.
	if(!enabled) state_ |= 0x100;
	else state_ &= ~0x100;
}

// MARK: - Active logic

void IWM::run_for(const Cycles cycles) {
}
