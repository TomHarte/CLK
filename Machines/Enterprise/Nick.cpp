//
//  Nick.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/06/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "Nick.hpp"

#include <cstdio>

using namespace Enterprise;

void Nick::write(uint16_t address, uint8_t value) {
	switch(address & 3) {
		default:
			printf("Unhandled Nick write: %02x -> %d\n", value, address & 3);
		break;
	}
}

uint8_t Nick::read([[maybe_unused]] uint16_t address) {
	return 0xff;
}

void Nick::run_for(HalfCycles) {

}
