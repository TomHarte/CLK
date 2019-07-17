//
//  Audio.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 31/05/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "Audio.hpp"

using namespace Apple::Macintosh;

namespace {

// The sample_length is coupled with the clock rate selected within the Macintosh proper.
const std::size_t sample_length = 352 / 2;

}

Audio::Audio(Concurrency::DeferringAsyncTaskQueue &task_queue) : task_queue_(task_queue) {}

// MARK: - Inputs

void Audio::post_sample(uint8_t sample) {
	// Store sample directly indexed by current write pointer; this ensures that collected samples
	// directly map to volume and enabled/disabled states.
	sample_queue_.buffer[sample_queue_.write_pointer] = sample;
	sample_queue_.write_pointer = (sample_queue_.write_pointer + 1) % sample_queue_.buffer.size();
}

void Audio::set_volume(int volume) {
	// Do nothing if the volume hasn't changed.
	if(posted_volume_ == volume) return;
	posted_volume_ = volume;

	// Post the volume change as a deferred event.
	task_queue_.defer([=] () {
		volume_ = volume;
	});
}

void Audio::set_enabled(bool on) {
	// Do nothing if the mask hasn't changed.
	if(posted_enable_mask_ == int(on)) return;
	posted_enable_mask_ = int(on);

	// Post the enabled mask change as a deferred event.
	task_queue_.defer([=] () {
		enabled_mask_ = int(on);
	});
}

// MARK: - Output generation

bool Audio::is_zero_level() {
	return !volume_ || !enabled_mask_;
}

void Audio::set_sample_volume_range(std::int16_t range) {
	// Some underflow here doesn't really matter.
	volume_multiplier_ = range / (7 * 255);
}

void Audio::get_samples(std::size_t number_of_samples, int16_t *target) {
	// TODO: the implementation below acts as if the hardware uses pulse-amplitude modulation;
	// in fact it uses pulse-width modulation. But the scale for pulses isn't specified, so
	// that's something to return to.

	// TODO: temporary implementation. Very inefficient. Replace.
	for(std::size_t sample = 0; sample < number_of_samples; ++sample) {
		target[sample] = volume_multiplier_ * int16_t(sample_queue_.buffer[sample_queue_.read_pointer] * volume_ * enabled_mask_);
		++subcycle_offset_;

		if(subcycle_offset_ == sample_length) {
			subcycle_offset_ = 0;
			sample_queue_.read_pointer = (sample_queue_.read_pointer + 1) % sample_queue_.buffer.size();
		}
	}
}
