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
	if(!number_of_drives_) return;

	// An Euler-esque approximation is used here: just collect all
	// the samples until there is a certain small quantity of them,
	// then produce a new estimate of rotation speed and start the
	// buffer afresh.
	samples_[sample_pointer_] = sample;
	++sample_pointer_;

	if(sample_pointer_ == samples_.size()) {
		sample_pointer_ = 0;

		// Treat 33 as a zero point and count zero crossings; then approximate
		// the RPM from the frequency of those.
		int samples_over = 0;
		const uint8_t centre = 33;
		for(size_t c = 0; c < 512; ++c) {
			if(samples_[c] > centre) ++ samples_over;
		}

		// TODO: if the above is the correct test, do it on sample receipt rather than
		// bothering with an intermediate buffer.

		// The below fits for a function like `a + bc`; I'm not sure it's
		const float duty_cycle = float(samples_over) / float(samples_.size());
		const float rotation_speed = 392.0f + duty_cycle * 19.95f;

		for(int c = 0; c < number_of_drives_; ++c) {
			drives_[c]->set_rotation_speed(rotation_speed);
		}
//		printf("RPM: %0.2f (%d over)\n", rotation_speed, samples_over);
	}
}

void DriveSpeedAccumulator::add_drive(Apple::Macintosh::DoubleDensityDrive *drive) {
	drives_[number_of_drives_] = drive;
	++number_of_drives_;
}
