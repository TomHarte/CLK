//
//  Audio.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 31/05/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "Audio.hpp"

using namespace Apple::Macintosh;

Audio::Audio(Concurrency::DeferringAsyncTaskQueue &task_queue) : task_queue_(task_queue) {}

// MARK: - Inputs

void Audio::post_sample(uint8_t sample) {
	const auto write_pointer = sample_queue_.write_pointer.load();
//	const auto pointers

	const auto read_pointer = sample_queue_.read_pointer.load();
}

void Audio::set_volume(int volume) {
	task_queue_.defer([=] () {
		volume_ = volume;
	});
}

void Audio::set_enabled(bool on) {
	task_queue_.defer([=] () {
		is_enabled_ = on;
	});
}

// MARK: - Output generation

bool Audio::is_zero_level() {
	return !volume_ || !is_enabled_;
}

void Audio::set_sample_volume_range(std::int16_t range) {
	volume_multiplier_ = range / 7;
}

void Audio::get_samples(std::size_t number_of_samples, int16_t *target) {
//	if(is_zero_level()) {
//		memset(target, 0, number_of_samples * sizeof(int16_t));
//	}
}
