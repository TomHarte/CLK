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
		int sum = 0;
		const uint8_t centre = 33;
		for(size_t c = 0; c < 512; ++c) {
			if(samples_[c] > centre) ++ samples_over;
			sum += samples_[c];
		}

		// TODO: if the above is the correct test, do it on sample receipt rather than
		// bothering with an intermediate buffer.

		// The below fits for a function like `a + bc`.
		const float rotation_speed = (float(sum) * 0.052896440564137f) - 259.0f;

		for(int c = 0; c < number_of_drives_; ++c) {
			drives_[c]->set_rotation_speed(rotation_speed);
		}
//		printf("RPM: %0.2f (%d over; %d sum)\n", rotation_speed, samples_over, sum);
	}
}

void DriveSpeedAccumulator::add_drive(Apple::Macintosh::DoubleDensityDrive *drive) {
	drives_[number_of_drives_] = drive;
	++number_of_drives_;
}
