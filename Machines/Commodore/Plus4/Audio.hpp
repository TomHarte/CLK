//
//  Audio.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 30/12/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#pragma once

#include "../../../Outputs/Speaker/Implementation/BufferSource.hpp"
#include "../../../Concurrency/AsyncTaskQueue.hpp"

namespace Commodore::Plus4 {

// PAL: / 160
// NTSC: / 128

// 111860.78125 = NTSC
// 110840.46875 = PAL

class Audio: public Outputs::Speaker::BufferSource<Audio, false> {
public:
	Audio(Concurrency::AsyncTaskQueue<false> &audio_queue) :
		audio_queue_(audio_queue) {}

	template <Outputs::Speaker::Action action>
	void apply_samples(std::size_t size, Outputs::Speaker::MonoSample *const target) {
		for(size_t c = 0; c < size; c++) {

			Outputs::Speaker::apply<action>(
				target[c],
				Outputs::Speaker::MonoSample(
					((r_ + c) & 128) ? external_volume_ : -external_volume_
				));
		}
		r_ += size;
	}

	void set_sample_volume_range(const std::int16_t range) {
		external_volume_ = range;
	}

	bool is_zero_level() const {
		return !(sound1_on_ || sound2_on_ || sound2_noise_on_) || !volume_;
	}

	template <int channel> void set_frequency_low(uint8_t value) {
		audio_queue_.enqueue([this, value] {
			frequencies_[channel] = (frequencies_[channel] & 0xff00) | value;
		});
	}

	template <int channel> void set_frequency_high(uint8_t value) {
		audio_queue_.enqueue([this, value] {
			frequencies_[channel] = (frequencies_[channel] & 0x00ff) | ((value&3) << 8);
		});
	}

	void set_constrol(uint8_t value) {
		audio_queue_.enqueue([this, value] {
			volume_ = value & 0xf;
			sound1_on_ = value & 0x10;
			sound2_on_ = value & 0x20;
			sound2_noise_on_ = value & 0x40;
			sound_dc_ = value & 0x80;
		});
	}

private:
	// Calling-thread state.
	Concurrency::AsyncTaskQueue<false> &audio_queue_;

	// Audio-thread state.
	int16_t external_volume_ = 0;
	int frequencies_[2]{};

	bool sound1_on_ = false;
	bool sound2_on_ = false;
	bool sound2_noise_on_ = false;
	bool sound_dc_ = false;
	uint8_t volume_ = 0;

	int r_ = 0;
};

}
