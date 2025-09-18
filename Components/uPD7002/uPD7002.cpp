//
//  uPD7002.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/09/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#include "uPD7002.hpp"

using namespace NEC;

uPD7002::uPD7002(HalfCycles) {
}

void uPD7002::run_for(HalfCycles) {
}

bool uPD7002::interrupt() const {
	return false;
}

void uPD7002::write(const uint16_t address, const uint8_t value) {
	(void)address;
	(void)value;
}

uint8_t uPD7002::read(const uint16_t address) {
	(void)address;
	return 0xff;
}
