//
//  MFP68901.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/10/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "MFP68901.hpp"

using namespace Motorola::MFP68901;

uint8_t MFP68901::read(int address) {
	/* TODO */
	return 0xff;
}

void MFP68901::write(int address, uint8_t value) {
	/* TODO */
}

void MFP68901::run_for(HalfCycles) {
	/* TODO */
}

HalfCycles MFP68901::get_next_sequence_point() {
	return HalfCycles(-1);
}
