//
//  Speaker.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/12/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "TIASound.hpp"

using namespace Atari2600;

Atari2600::TIASound::TIASound(Concurrency::AsyncTaskQueue<false> &audio_queue) :
	audio_queue_(audio_queue)
{}

void Atari2600::TIASound::set_volume(const int channel, const uint8_t volume) {
	audio_queue_.enqueue([target = &volume_[channel], volume]() {
		*target = volume & 0xf;
	});
}

void Atari2600::TIASound::set_divider(const int channel, const uint8_t divider) {
	audio_queue_.enqueue([this, channel, divider]() {
		divider_[channel] = divider & 0x1f;
		divider_counter_[channel] = 0;
	});
}

void Atari2600::TIASound::set_control(const int channel, const uint8_t control) {
	audio_queue_.enqueue([target = &control_[channel], control]() {
		*target = control & 0xf;
	});
}

template <Outputs::Speaker::Action action>
void Atari2600::TIASound::apply_samples(
	const std::size_t number_of_samples,
	Outputs::Speaker::MonoSample *const target
) {
	const auto advance_poly4 = [&](const int channel) {
		poly4_counter_[channel] =
			(poly4_counter_[channel] >> 1) |
			(((poly4_counter_[channel] << 3) ^ (poly4_counter_[channel] << 2))&0x008);
	};
	const auto advance_poly5 = [&](const int channel) {
		poly5_counter_[channel] =
			(poly5_counter_[channel] >> 1) |
			(((poly5_counter_[channel] << 4) ^ (poly5_counter_[channel] << 2))&0x010);
	};
	const auto advance_poly9 = [&](const int channel) {
		poly9_counter_[channel] =
			(poly9_counter_[channel] >> 1) |
			(((poly9_counter_[channel] << 4) ^ (poly9_counter_[channel] << 8))&0x100);
	};

	for(unsigned int c = 0; c < number_of_samples; c++) {
		Outputs::Speaker::MonoSample output = 0;
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

			output += (volume_[channel] * per_channel_volume_ * level) >> 4;
		}
		Outputs::Speaker::apply<action>(target[c], output);
	}
}

template void Atari2600::TIASound::apply_samples<Outputs::Speaker::Action::Mix>(
	std::size_t, Outputs::Speaker::MonoSample *);
template void Atari2600::TIASound::apply_samples<Outputs::Speaker::Action::Store>(
	std::size_t, Outputs::Speaker::MonoSample *);
template void Atari2600::TIASound::apply_samples<Outputs::Speaker::Action::Ignore>(
	std::size_t, Outputs::Speaker::MonoSample *);

void Atari2600::TIASound::set_sample_volume_range(const std::int16_t range) {
	per_channel_volume_ = range / 2;
}
