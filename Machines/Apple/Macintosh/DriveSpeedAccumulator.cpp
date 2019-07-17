//
//  DriveSpeedAccumulator.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 01/06/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "DriveSpeedAccumulator.hpp"

using namespace Apple::Macintosh;

void DriveSpeedAccumulator::post_sample(uint8_t sample) {
	// An Euler-esque approximation is used here: just collect all
	// the samples until there is a certain small quantity of them,
	// then produce a new estimate of rotation speed and start the
	// buffer afresh.
	samples_[sample_pointer_] = sample;
	++sample_pointer_;

	if(sample_pointer_ == samples_.size()) {
		sample_pointer_ = 0;
//		for(int c = 0; c < 512; c += 32) {
//			printf("%u ", samples_[c]);
//		}
//		printf("\n");
	}
}
