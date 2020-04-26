//
//  LowFrequencyOscillator.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/04/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#include "LowFrequencyOscillator.hpp"

using namespace Yamaha::OPL;

void LowFrequencyOscillator::update() {
	++counter;

	// This produces output of:
	//
	// four instances of 0, four instances of 1... _three_ instances of 26,
	// four instances of 25, four instances of 24... _three_ instances of 0.
	//
	// ... advancing once every 64th update.
	const int tremolo_index = (counter >> 6) % 210;
	const int tremolo_levels[2] = {tremolo_index >> 2, 52 - ((tremolo_index+1) >> 2)};
	tremolo = tremolo_levels[tremolo_index / 107];

	// Vibrato is relatively simple: it's just three bits from the counter.
	vibrato = (counter >> 10) & 7;
}

void LowFrequencyOscillator::update_lfsr() {
	lfsr = noise_source_.next();
}
