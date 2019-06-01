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
const HalfCycles sample_length(704);
}

Audio::Audio(Concurrency::DeferringAsyncTaskQueue &task_queue) : task_queue_(task_queue) {}

// MARK: - Inputs

void Audio::post_sample(uint8_t sample) {
	// Grab the read and write pointers, ensure there's room for a new sample and, if not,
	// drop this one.
	const auto write_pointer = sample_queue_.write_pointer.load();
	const auto read_pointer = sample_queue_.read_pointer.load();
	const decltype(write_pointer) next_write_pointer = (write_pointer + 1) % sample_queue_.buffer.size();
	if(next_write_pointer == read_pointer) return;

	sample_queue_.buffer[write_pointer] = sample;
	sample_queue_.write_pointer.store(next_write_pointer);
}

void Audio::set_volume(int volume) {
	// Post the volume change as a deferred event.
	task_queue_.defer([=] () {
		volume_ = volume;
	});
}

void Audio::set_enabled(bool on) {
	// Post the enabled mask change as a deferred event.
	task_queue_.defer([=] () {
		enabled_mask_ = on ? 1 : 0;
	});
}

// MARK: - Output generation

bool Audio::is_zero_level() {
	return !volume_ || !enabled_mask_;
}

void Audio::set_sample_volume_range(std::int16_t range) {
	// Some underflow here doesn't really matter.
	volume_multiplier_ = range / 7;
}

void Audio::get_samples(std::size_t number_of_samples, int16_t *target) {
	// TODO.
}

void Audio::skip_samples(std::size_t number_of_samples) {
	// TODO.
}
