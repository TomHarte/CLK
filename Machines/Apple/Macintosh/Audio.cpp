//
//  Audio.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 31/05/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "Audio.hpp"

#include <algorithm>

using namespace Apple::Macintosh;

namespace {
constexpr HalfCycles MasterClockDivider = 8;	// Period at which to generate output samples; higher than the sampling
												// rate to capture VIA activity — volume changes and the enable/disable
												// toggle — which might occur at higher frequencies.

constexpr auto MaxBufferSize = 1024;			// Maximum number of samples to accumulate on the emulator thread before
												// pushing to the speaker.
}

// MARK: - Setup
Audio::Audio(HalfCycles clock_rate, const Video &video, DriveSpeedAccumulator &drive_speed_accumulator) :
	phase_(video.audio_phase()),
	drive_speed_accumulator_(drive_speed_accumulator) {

	speaker_.speaker().set_input_rate(clock_rate.as<float>() / MasterClockDivider.as<float>());
}

void Audio::set_ram(const uint16_t *const ram, const uint32_t mask) {
	ram_ = ram;
	ram_mask_ = mask;
}

void Audio::set_use_alternate_buffer(const bool use_alternate_buffer) {
	use_alternate_buffer_ = use_alternate_buffer;
}

// MARK: - Inputs

void Audio::operator += (const HalfCycles duration) {
	HalfCycles remaining = duration;
	while(remaining > HalfCycles(0)) {
		const auto next = std::min(remaining.as<unsigned>(), phase_.sample_phase.remaining());
		remaining -= HalfCycles(next);
		time_ += HalfCycles(next);

		if(phase_.sample_phase.advance_divide(duration.as<unsigned>())) {
			const size_t base =
				use_alternate_buffer_ ? (0xffffa100 >> 1) : (0xfffffd00 >> 1);
			const uint16_t audio_word = ram_[(address_ + base) & ram_mask_];
			++address_;

			post_sample(audio_word >> 8);
			drive_speed_accumulator_.post_sample(audio_word & 0xff);

			if(phase_.buffer_phase.advance_divide(1)) {
				address_ = 0;
			}
		}

		static constexpr auto MaxBufferDuration = HalfCycles(Cycles(MaxBufferSize)) * MasterClockDivider;
		if(time_ >= MaxBufferDuration) {
			generate();
		}
	}
}

void Audio::generate() {
	const auto samples = time_.divide(MasterClockDivider).as<size_t>();
	const auto level = int16_t(sample_ * volume_multiplier_);

	std::fill_n(std::back_inserter(samples_), samples, level);
	if(samples_.size() >= MaxBufferSize) {
		flush();
	}
}

void Audio::flush() {
	speaker_.push_swap(samples_);
}

// MARK: - Setters.

void Audio::post_sample(const uint8_t sample) {
	if(sample_ == sample) return;
	generate();

	sample_ = sample;
}

void Audio::set_volume(const int volume) {
	if(volume_ == volume) return;
	generate();

	volume_ = volume;
	set_volume_multiplier();
}

void Audio::set_enabled(const bool on) {
	if(enable_mask_ == int(on)) return;
	generate();

	enable_mask_ = int(on);
	set_volume_multiplier();
}

void Audio::set_volume_multiplier() {
	volume_multiplier_ = int16_t(volume_ * enable_mask_ * (127 / 7));
}
