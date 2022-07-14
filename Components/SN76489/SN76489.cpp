//
//  SN76489.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 26/02/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#include "SN76489.hpp"

#include <cassert>
#include <cmath>

using namespace TI;

SN76489::SN76489(Personality personality, Concurrency::TaskQueue<false> &task_queue, int additional_divider) : task_queue_(task_queue) {
	set_sample_volume_range(0);

	switch(personality) {
		case Personality::SN76494:
			master_divider_period_ = 2;
			shifter_is_16bit_ = false;
		break;
		case Personality::SN76489:
			master_divider_period_ = 16;
			shifter_is_16bit_ = false;
		break;
		case Personality::SMS:
			master_divider_period_ = 16;
			shifter_is_16bit_ = true;
		break;
	}

	assert((master_divider_period_ % additional_divider) == 0);
	assert(additional_divider < master_divider_period_);
	master_divider_period_ /= additional_divider;
}

void SN76489::set_sample_volume_range(std::int16_t range) {
	// Build a volume table.
	double multiplier = pow(10.0, -0.1);
	double volume = float(range) / 4.0f;	// As there are four channels.
	for(int c = 0; c < 16; ++c) {
		volumes_[c] = int(round(volume));
		volume *= multiplier;
	}
	volumes_[15] = 0;
	evaluate_output_volume();
}

void SN76489::write(uint8_t value) {
	task_queue_.enqueue([value, this] () {
		if(value & 0x80) {
			active_register_ = value;
		}

		const int channel = (active_register_ >> 5)&3;
		if(active_register_ & 0x10) {
			// latch for volume
			channels_[channel].volume = value & 0xf;
			evaluate_output_volume();
		} else {
			// latch for tone/data
			if(channel < 3) {
				if(value & 0x80) {
					channels_[channel].divider = (channels_[channel].divider & ~0xf) | (value & 0xf);
				} else {
					channels_[channel].divider = uint16_t((channels_[channel].divider & 0xf) | ((value & 0x3f) << 4));
				}
			} else {
				// writes to the noise register always reset the shifter
				noise_shifter_ = shifter_is_16bit_ ? 0x8000 : 0x4000;

				if(value & 4) {
					noise_mode_ = shifter_is_16bit_ ? Noise16 : Noise15;
				} else {
					noise_mode_ = shifter_is_16bit_ ? Periodic16 : Periodic15;
				}

				channels_[3].divider = uint16_t(0x10 << (value & 3));
				// Special case: if these bits are both set, the noise channel should track channel 2,
				// which is marked with a divider of 0xffff.
				if(channels_[3].divider == 0x80) channels_[3].divider = 0xffff;
			}
		}
	});
}

bool SN76489::is_zero_level() const {
	return channels_[0].volume == 0xf && channels_[1].volume == 0xf && channels_[2].volume == 0xf && channels_[3].volume == 0xf;
}

void SN76489::evaluate_output_volume() {
	output_volume_ = int16_t(
		channels_[0].level * volumes_[channels_[0].volume] +
		channels_[1].level * volumes_[channels_[1].volume] +
		channels_[2].level * volumes_[channels_[2].volume] +
		channels_[3].level * volumes_[channels_[3].volume]
	);
}

void SN76489::get_samples(std::size_t number_of_samples, std::int16_t *target) {
	std::size_t c = 0;
	while((master_divider_& (master_divider_period_ - 1)) && c < number_of_samples) {
		target[c] = output_volume_;
		master_divider_++;
		c++;
	}

	while(c < number_of_samples) {
		bool did_flip = false;

#define step_channel(x, s) \
		if(channels_[x].counter) channels_[x].counter--;\
		else {\
			channels_[x].level ^= 1;\
			channels_[x].counter = channels_[x].divider;\
			s;\
		}

		step_channel(0, /**/);
		step_channel(1, /**/);
		step_channel(2, did_flip = true);

#undef step_channel

		if(channels_[3].divider != 0xffff) {
			if(channels_[3].counter) channels_[3].counter--;
			else {
				did_flip = true;
				channels_[3].counter = channels_[3].divider;
			}
		}

		if(did_flip) {
			channels_[3].level = noise_shifter_ & 1;
			int new_bit = channels_[3].level;
			switch(noise_mode_) {
				default: break;
				case Noise15:
					new_bit ^= (noise_shifter_ >> 1);
				break;
				case Noise16:
					new_bit ^= (noise_shifter_ >> 3);
				break;
			}
			noise_shifter_ >>= 1;
			noise_shifter_ |= (new_bit & 1) << (shifter_is_16bit_ ? 15 : 14);
		}

		evaluate_output_volume();

		for(int ic = 0; ic < master_divider_period_ && c < number_of_samples; ++ic) {
			target[c] = output_volume_;
			c++;
			master_divider_++;
		}
	}

	master_divider_ &= (master_divider_period_ - 1);
}
