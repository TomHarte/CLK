//
//  Audio.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 30/12/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#pragma once

#include "Outputs/Speaker/Implementation/BufferSource.hpp"
#include "Concurrency/AsyncTaskQueue.hpp"

namespace Commodore::Plus4 {

class Audio: public Outputs::Speaker::BufferSource<Audio, false> {
public:
	Audio(Concurrency::AsyncTaskQueue<false> &audio_queue) :
		audio_queue_(audio_queue) {}

	template <Outputs::Speaker::Action action>
	void apply_samples(std::size_t size, Outputs::Speaker::MonoSample *const target) {
		// Divide by frequency_multiplier_ and multiply by 8 to get the "8Mhz clock".
		// Each 32-window cycle of that clock is a single complete tick of the audio engine.

		const auto update_channel = [&](int channel) {
			if(sound_dc_ || channels_[channel].count == 0x3ff) {
				channels_[channel].count = (channels_[channel].frequency + 1) & 0x3ff;
			} else {
				++channels_[channel].count;
				if(channels_[channel].count == 0x3ff) {
					channels_[channel].state ^= 1;
				}
			}

			channels_[channel].pwm_count = 0;
		};

		const auto update_noise = [&] {
			noise_ <<= 1;
			noise_ |= 1 ^ (noise_ >> 7) ^ (noise_ >> 5) ^ (noise_ >> 4) ^ (noise_ >> 1);
		};

		for(size_t c = 0; c < size; c++) {
			subcycle_ = (subcycle_ + 1) & 31;

			if(subcycle_ == 0) update_channel(0);
			if(subcycle_ == 16) {
				update_channel(1);
				update_noise();
			}

			++ channels_[0].pwm_count;
			++ channels_[1].pwm_count;

			if(sound_dc_) {
				channels_[0].state = 0;
				channels_[1].state = 0;
				noise_ = 0;
			}

			target[c] =
				external_volume_ * (
					((channels_[0].pwm_count < volume_) & channels_[0].enabled & ~channels_[0].state) |
					((channels_[1].pwm_count < volume_) &
						(
							(channels_[1].enabled & ~channels_[1].state) |
							(sound2_noise_on_ & noise_ & 1)
						)
					)
				);
		}
	}

	void set_sample_volume_range(const std::int16_t range) {
		external_volume_ = range; // / (2 * 9);	// Two channels and nine output levels.
	}

	bool is_zero_level() const {
		return !(channels_[0].enabled || channels_[1].enabled || sound2_noise_on_) || !volume_;
	}

	template <int channel> void set_frequency_low(uint8_t value) {
		audio_queue_.enqueue([this, value] {
			channels_[channel].frequency = (channels_[channel].frequency & 0xff00) | value;
		});
	}

	template <int channel> void set_frequency_high(uint8_t value) {
		audio_queue_.enqueue([this, value] {
			channels_[channel].frequency = (channels_[channel].frequency & 0x00ff) | ((value&3) << 8);
		});
	}

	void set_control(const uint8_t value) {
		audio_queue_.enqueue([this, value] {
			switch(value & 0xf) {
				case 0:		volume_ = 31;	break;
				case 1:		volume_ = 30;	break;
				case 2:		volume_ = 28;	break;
				case 3:		volume_ = 26;	break;
				case 4:		volume_ = 24;	break;
				case 5:		volume_ = 22;	break;
				case 6:		volume_ = 20;	break;
				case 7:		volume_ = 18;	break;
				default:	volume_ = 16;	break;
			}
			volume_ = std::min(value & 0xf, 8);	// Only nine volumes are available.
			channels_[0].enabled = (value & 0x10) ? 1 : 0;
			channels_[1].enabled = (value & 0x20) ? 1 : 0;
			sound2_noise_on_ = (value & 0x40) && !(value & 0x20);

			sound_dc_ = value & 0x80;
		});
	}

	void set_divider(const uint8_t value) {
		audio_queue_.enqueue([this, value] {
			frequency_multiplier_ = 32 * (value & 0x40 ? 4 : 5);
		});
	}

private:
	// Calling-thread state.
	Concurrency::AsyncTaskQueue<false> &audio_queue_;

	// Audio-thread state.
	int16_t external_volume_ = 0;
	int frequency_multiplier_ = 5;
	int subcycle_ = 0;
	struct Channel {
		int frequency = 0;
		int count = 0;
		int pwm_count = 0;
		int state = 0;
		int enabled = 0;
	} channels_[2];
	uint8_t noise_ = 0;

	bool sound2_noise_on_ = false;
	bool sound_dc_ = false;
	int volume_ = 0;
};

}
