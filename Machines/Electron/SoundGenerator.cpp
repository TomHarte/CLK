//
//  Speaker.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/12/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "SoundGenerator.hpp"

using namespace Electron;

SoundGenerator::SoundGenerator(Concurrency::DeferringAsyncTaskQueue &audio_queue) :
	audio_queue_(audio_queue) {}

void SoundGenerator::get_samples(std::size_t number_of_samples, int16_t *target) {
	if(is_enabled_) {
		while(number_of_samples--) {
			*target = static_cast<int16_t>((counter_ / (divider_+1)) * 8192);
			target++;
			counter_ = (counter_ + 1) % ((divider_+1) * 2);
		}
	} else {
		memset(target, 0, sizeof(int16_t) * number_of_samples);
	}
}

void SoundGenerator::skip_samples(std::size_t number_of_samples) {
	counter_ = (counter_ + number_of_samples) % ((divider_+1) * 2);
}

void SoundGenerator::set_divider(uint8_t divider) {
	audio_queue_.defer([=]() {
		divider_ = divider * 32 / clock_rate_divider;
	});
}

void SoundGenerator::set_is_enabled(bool is_enabled) {
	audio_queue_.defer([=]() {
		is_enabled_ = is_enabled;
		counter_ = 0;
	});
}
