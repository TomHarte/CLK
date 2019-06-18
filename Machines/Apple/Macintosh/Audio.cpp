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
const std::size_t sample_length = 352;
}

Audio::Audio(Concurrency::DeferringAsyncTaskQueue &task_queue) : task_queue_(task_queue) {}

// MARK: - Inputs

void Audio::post_sample(uint8_t sample) {
	// Grab the read and write pointers, ensure there's room for a new sample and, if not,
	// drop this one.
	const auto write_pointer = sample_queue_.write_pointer.load();
	const auto read_pointer = sample_queue_.read_pointer.load();
	const decltype(write_pointer) next_write_pointer = (write_pointer + 1) % sample_queue_.buffer.size();
	if(next_write_pointer == read_pointer) {
		return;
	}

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
	volume_multiplier_ = range / (7 * 255);
}

void Audio::get_samples(std::size_t number_of_samples, int16_t *target) {
	const auto write_pointer = sample_queue_.write_pointer.load();
	auto read_pointer = sample_queue_.read_pointer.load();

	// TODO: the implementation below acts as if the hardware uses pulse-amplitude modulation;
	// in fact it uses pulse-width modulation. But the scale for pulses isn't specified, so
	// that's something to return to.

	// TODO: temporary implementation. Very inefficient. Replace.
	for(std::size_t sample = 0; sample < number_of_samples; ++sample) {
//		if(volume_ && enabled_mask_) printf("%d\n", sample_queue_.buffer[read_pointer]);
		target[sample] = volume_multiplier_ * int16_t(sample_queue_.buffer[read_pointer] * volume_ * enabled_mask_);
		++subcycle_offset_;

		if(subcycle_offset_ == sample_length) {
//			printf("%d: %d\n", sample_queue_.buffer[read_pointer], volume_multiplier_ * int16_t(sample_queue_.buffer[read_pointer]));
			subcycle_offset_ = 0;
			const unsigned int next_read_pointer = (read_pointer + 1) % sample_queue_.buffer.size();
			if(next_read_pointer != write_pointer) {
				read_pointer = next_read_pointer;
			}
		}
	}

	sample_queue_.read_pointer.store(read_pointer);
}

void Audio::skip_samples(std::size_t number_of_samples) {
	const auto write_pointer = sample_queue_.write_pointer.load();
	auto read_pointer = sample_queue_.read_pointer.load();

	// Number of samples that would be consumed is (number_of_samples + subcycle_offset_) / sample_length.
	const unsigned int samples_passed = static_cast<unsigned int>((number_of_samples + subcycle_offset_) / sample_length);
	subcycle_offset_ = (number_of_samples + subcycle_offset_) % sample_length;

	// Get also number of samples available.
	const unsigned int samples_available = static_cast<unsigned int>((write_pointer + sample_queue_.buffer.size() - read_pointer) % sample_queue_.buffer.size());

	// Advance by whichever of those is the lower number.
	const auto samples_to_consume = std::min(samples_available, samples_passed);
	read_pointer = (read_pointer + samples_to_consume) % sample_queue_.buffer.size();

	sample_queue_.read_pointer.store(read_pointer);
}
