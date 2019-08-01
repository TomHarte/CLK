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

		// The below fits for a function like `a + bc`; it encapsultes the following
		// beliefs:
		//
		//	(i) motor speed is proportional to voltage supplied;
		//	(ii) with pulse-width modulation it's therefore proportional to the duty cycle;
		//	(iii) the Mac pulse-width modulates whatever it reads from the disk speed buffer;
		//	(iv) ... subject to software pulse-width modulation of that pulse-width modulation.
		//
		// So, I believe current motor speed is proportional to a low-pass filtering of
		// the speed buffer. Which I've implemented very coarsely via 'large' bucketed-averages,
		// noting also that exact disk motor speed is always a little approximate.

		// Sum all samples.
		// TODO: if the above is the correct test, do it on sample receipt rather than
		// bothering with an intermediate buffer.
		int sum = 0;
		for(auto s: samples_) {
			sum += s;
		}

		// The formula below was derived from observing values the Mac wrote into its
		// disk-speed buffer. Given that it runs a calibration loop before doing so,
		// I cannot guarantee the accuracy of these numbers beyond being within the
		// range that the computer would accept.
		const float normalised_sum = float(sum) / float(samples_.size());
		const float rotation_speed = (normalised_sum * 27.08f) - 259.0f;

		for(int c = 0; c < number_of_drives_; ++c) {
			drives_[c]->set_rotation_speed(rotation_speed);
		}
//		printf("RPM: %0.2f (%d sum)\n", rotation_speed, sum);
	}
}

void DriveSpeedAccumulator::add_drive(Apple::Macintosh::DoubleDensityDrive *drive) {
	drives_[number_of_drives_] = drive;
	++number_of_drives_;
}
