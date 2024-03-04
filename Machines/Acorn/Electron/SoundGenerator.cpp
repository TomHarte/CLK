//
//  Speaker.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/12/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "SoundGenerator.hpp"

#include <cstring>

using namespace Electron;

SoundGenerator::SoundGenerator(Concurrency::AsyncTaskQueue<false> &audio_queue) :
	audio_queue_(audio_queue) {}

void SoundGenerator::set_sample_volume_range(std::int16_t range) {
	volume_ = unsigned(range / 2);
}

template <Outputs::Speaker::Action action>
void SoundGenerator::apply_samples(std::size_t number_of_samples, Outputs::Speaker::MonoSample *target) {
	if constexpr (action == Outputs::Speaker::Action::Ignore) {
		counter_ = (counter_ + number_of_samples) % ((divider_+1) * 2);
		return;
	}

	if(is_enabled_) {
		while(number_of_samples--) {
			Outputs::Speaker::apply<action>(*target, Outputs::Speaker::MonoSample((counter_ / (divider_+1)) * volume_));
			target++;
			counter_ = (counter_ + 1) % ((divider_+1) * 2);
		}
	} else {
		Outputs::Speaker::fill<action>(target, target + number_of_samples, Outputs::Speaker::MonoSample(0));
	}
}
template void SoundGenerator::apply_samples<Outputs::Speaker::Action::Mix>(std::size_t, Outputs::Speaker::MonoSample *);
template void SoundGenerator::apply_samples<Outputs::Speaker::Action::Store>(std::size_t, Outputs::Speaker::MonoSample *);
template void SoundGenerator::apply_samples<Outputs::Speaker::Action::Ignore>(std::size_t, Outputs::Speaker::MonoSample *);

void SoundGenerator::set_divider(uint8_t divider) {
	audio_queue_.enqueue([this, divider]() {
		divider_ = divider * 32 / clock_rate_divider;
	});
}

void SoundGenerator::set_is_enabled(bool is_enabled) {
	audio_queue_.enqueue([this, is_enabled]() {
		is_enabled_ = is_enabled;
		counter_ = 0;
	});
}
