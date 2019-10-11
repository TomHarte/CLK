//
//  6850.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/10/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "6850.hpp"

using namespace Motorola::ACIA;

uint8_t ACIA::read(int address) {
	return 0xff;
}

void ACIA::write(int address, uint8_t value) {
}

void ACIA::run_for(HalfCycles) {
}
