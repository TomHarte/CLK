//
//  6850.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/10/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "6850.hpp"

#define LOG_PREFIX "[6850] "
#include "../../Outputs/Log.hpp"

using namespace Motorola::ACIA;

uint8_t ACIA::read(int address) {
	if(address&1) {
		LOG("Read from receive register");
	} else {
		LOG("Read status");
		return status_;
	}
	return 0x00;
}

void ACIA::write(int address, uint8_t value) {
	if(address&1) {
		LOG("Write to transmit register");
	} else {
		if((value&3) == 3) {
			LOG("Reset");
		} else {
			LOG("Write to control register");
		}
	}
}

void ACIA::run_for(HalfCycles) {
}
