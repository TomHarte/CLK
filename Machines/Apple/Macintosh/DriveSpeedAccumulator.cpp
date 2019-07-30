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

		// Treat 35 as a zero point and count zero crossings; then approximate
		// the RPM from the frequency of those.
		size_t first_crossing = 0, last_crossing = 0;
		int number_of_crossings = 0;

		const uint8_t centre = 35;
		bool was_over = samples_[0] > centre;
		for(size_t c = 1; c < 512; ++c) {
			bool is_over = samples_[c] > centre;
			if(is_over != was_over) {
				if(!first_crossing) first_crossing = c;
				last_crossing = c;
				++number_of_crossings;
			}
			was_over = is_over;
		}

		if(number_of_crossings) {
			// The 654 multiplier here is a complete guess, based on preliminary
			// observations of the values supplied and the known RPM selections of
			// the 800kb drive. Updated values may be needed.
			const float rotation_speed = 654.0f * float(number_of_crossings) / float(last_crossing - first_crossing);
			for(int c = 0; c < number_of_drives_; ++c) {
				drives_[c]->set_rotation_speed(rotation_speed);
			}
		}
	}
}

void DriveSpeedAccumulator::add_drive(Apple::Macintosh::DoubleDensityDrive *drive) {
	drives_[number_of_drives_] = drive;
	++number_of_drives_;
}
