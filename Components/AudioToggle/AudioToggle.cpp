//
//  AudioToggle.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/04/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#include "AudioToggle.hpp"

#include <algorithm>

using namespace Audio;

Audio::DAC::DAC(Concurrency::AsyncTaskQueue<false> &audio_queue, const int16_t max) :
	audio_queue_(audio_queue), max_output_(max) {}

void DAC::update_level() {
	level_ = (output_ * volume_) / max_output_;
}

void DAC::set_sample_volume_range(const std::int16_t range) {
	volume_ = range;
	update_level();
}

void DAC::set_output(const int16_t output) {
	if(set_output_ == output) return;
	set_output_ = output;

	audio_queue_.enqueue([this, output] {
		output_ = output;
		update_level();
	});
}

int16_t DAC::get_output() const {
	return set_output_;
}
