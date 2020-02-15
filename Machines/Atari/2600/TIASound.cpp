//
//  Speaker.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/12/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "TIASound.hpp"

using namespace Atari2600;

Atari2600::TIASound::TIASound(Concurrency::DeferringAsyncTaskQueue &audio_queue) :
	audio_queue_(audio_queue),
	poly4_counter_{0x00f, 0x00f},
	poly5_counter_{0x01f, 0x01f},
	poly9_counter_{0x1ff, 0x1ff}
{}

void Atari2600::TIASound::set_volume(int channel, uint8_t volume) {
	audio_queue_.defer([target = &volume_[channel], volume]() {
		*target = volume & 0xf;
	});
}

void Atari2600::TIASound::set_divider(int channel, uint8_t divider) {
	audio_queue_.defer([this, channel, divider]() {
		divider_[channel] = divider & 0x1f;
		divider_counter_[channel] = 0;
	});
}

void Atari2600::TIASound::set_control(int channel, uint8_t control) {
	audio_queue_.defer([target = &control_[channel], control]() {
		*target = control & 0xf;
	});
}

#define advance_poly4(c) poly4_counter_[channel] = (poly4_counter_[channel] >> 1) | (((poly4_counter_[channel] << 3) ^ (poly4_counter_[channel] << 2))&0x008)
#define advance_poly5(c) poly5_counter_[channel] = (poly5_counter_[channel] >> 1) | (((poly5_counter_[channel] << 4) ^ (poly5_counter_[channel] << 2))&0x010)
#define advance_poly9(c) poly9_counter_[channel] = (poly9_counter_[channel] >> 1) | (((poly9_counter_[channel] << 4) ^ (poly9_counter_[channel] << 8))&0x100)

void Atari2600::TIASound::get_samples(std::size_t number_of_samples, int16_t *target) {
	for(unsigned int c = 0; c < number_of_samples; c++) {
		target[c] = 0;
		for(int channel = 0; channel < 2; channel++) {
			divider_counter_[channel] ++;
			int divider_value = divider_counter_[channel] / (38 / CPUTicksPerAudioTick);
			int level = 0;
			switch(control_[channel]) {
				case 0x0: case 0xb:	// constant 1
					level = 1;
				break;

				case 0x4: case 0x5:	// div2 tone
					level = (divider_value / (divider_[channel]+1))&1;
				break;

				case 0xc: case 0xd:	// div6 tone
					level = (divider_value / ((divider_[channel]+1)*3))&1;
				break;

				case 0x6: case 0xa:	// div31 tone
					level = (divider_value / (divider_[channel]+1))%30 <= 18;
				break;

				case 0xe:			// div93 tone
					level = (divider_value / ((divider_[channel]+1)*3))%30 <= 18;
				break;

				case 0x1:			// 4-bit poly
					level = poly4_counter_[channel]&1;
					if(divider_value == divider_[channel]+1) {
						divider_counter_[channel] = 0;
						advance_poly4(channel);
					}
				break;

				case 0x2:			// 4-bit poly div31
					level = poly4_counter_[channel]&1;
					if(divider_value%(30*(divider_[channel]+1)) == 18) {
						advance_poly4(channel);
					}
				break;

				case 0x3:			// 5/4-bit poly
					level = output_state_[channel];
					if(divider_value == divider_[channel]+1) {
						if(poly5_counter_[channel]&1) {
							output_state_[channel] = poly4_counter_[channel]&1;
							advance_poly4(channel);
						}
						advance_poly5(channel);
					}
				break;

				case 0x7: case 0x9:	// 5-bit poly
					level = poly5_counter_[channel]&1;
					if(divider_value == divider_[channel]+1) {
						divider_counter_[channel] = 0;
						advance_poly5(channel);
					}
				break;

				case 0xf:			// 5-bit poly div6
					level = poly5_counter_[channel]&1;
					if(divider_value == (divider_[channel]+1)*3) {
						divider_counter_[channel] = 0;
						advance_poly5(channel);
					}
				break;

				case 0x8:			// 9-bit poly
					level = poly9_counter_[channel]&1;
					if(divider_value == divider_[channel]+1) {
						divider_counter_[channel] = 0;
						advance_poly9(channel);
					}
				break;
			}

			target[c] += (volume_[channel] * per_channel_volume_ * level) >> 4;
		}
	}
}

void Atari2600::TIASound::set_sample_volume_range(std::int16_t range) {
	per_channel_volume_ = range / 2;
}
