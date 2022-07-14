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

// The sample_length is coupled with the clock rate selected within the Macintosh proper;
// as per the header-declaration a divide-by-two clock is expected to arrive here.
const std::size_t sample_length = 352 / 2;

}

Audio::Audio(Concurrency::TaskQueue<false> &task_queue) : task_queue_(task_queue) {}

// MARK: - Inputs

void Audio::post_sample(uint8_t sample) {
	// Store sample directly indexed by current write pointer; this ensures that collected samples
	// directly map to volume and enabled/disabled states.
	sample_queue_.buffer[sample_queue_.write_pointer].store(sample, std::memory_order::memory_order_relaxed);
	sample_queue_.write_pointer = (sample_queue_.write_pointer + 1) % sample_queue_.buffer.size();
}

void Audio::set_volume(int volume) {
	// Do nothing if the volume hasn't changed.
	if(posted_volume_ == volume) return;
	posted_volume_ = volume;

	// Post the volume change as a deferred event.
	task_queue_.enqueue([this, volume] () {
		volume_ = volume;
		set_volume_multiplier();
	});
}

void Audio::set_enabled(bool on) {
	// Do nothing if the mask hasn't changed.
	if(posted_enable_mask_ == int(on)) return;
	posted_enable_mask_ = int(on);

	// Post the enabled mask change as a deferred event.
	task_queue_.enqueue([this, on] () {
		enabled_mask_ = int(on);
		set_volume_multiplier();
	});
}

// MARK: - Output generation

bool Audio::is_zero_level() const {
	return !volume_ || !enabled_mask_;
}

void Audio::set_sample_volume_range(std::int16_t range) {
	// Some underflow here doesn't really matter.
	output_volume_ = range / (7 * 255);
	set_volume_multiplier();
}

void Audio::set_volume_multiplier() {
	volume_multiplier_ = int16_t(output_volume_ * volume_ * enabled_mask_);
}

void Audio::get_samples(std::size_t number_of_samples, int16_t *target) {
	// TODO: the implementation below acts as if the hardware uses pulse-amplitude modulation;
	// in fact it uses pulse-width modulation. But the scale for pulses isn't specified, so
	// that's something to return to.

	while(number_of_samples) {
		// Determine how many output samples will be at the same level.
		const auto cycles_left_in_sample = std::min(number_of_samples, sample_length - subcycle_offset_);

		// Determine the output level, and output that many samples.
		// (Hoping that the copiler substitutes an effective memset16-type operation here).
		const int16_t output_level = volume_multiplier_ * (int16_t(sample_queue_.buffer[sample_queue_.read_pointer].load(std::memory_order::memory_order_relaxed)) - 128);
		for(size_t c = 0; c < cycles_left_in_sample; ++c) {
			target[c] = output_level;
		}
		target += cycles_left_in_sample;

		// Advance the sample pointer.
		subcycle_offset_ += cycles_left_in_sample;
		sample_queue_.read_pointer = (sample_queue_.read_pointer + (subcycle_offset_ / sample_length)) % sample_queue_.buffer.size();
		subcycle_offset_ %= sample_length;

		// Decreate the number of samples left to write.
		number_of_samples -= cycles_left_in_sample;
	}
}
