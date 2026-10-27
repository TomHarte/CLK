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
constexpr auto MaxBufferSize = 1024;
constexpr auto MaxBufferDuration = HalfCycles(Cycles(MaxBufferSize)) * MasterClockDivider;
}

// MARK: - Inputs

void Audio::operator += (const HalfCycles duration) {
	time_ += duration;

	if(time_ >= MaxBufferDuration) {
		generate();
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
