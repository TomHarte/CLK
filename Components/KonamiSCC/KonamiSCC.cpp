//
//  KonamiSCC.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/01/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#include "KonamiSCC.hpp"

#include <cstring>

using namespace Konami;

SCC::SCC(Concurrency::TaskQueue<false> &task_queue) :
	task_queue_(task_queue) {}

bool SCC::is_zero_level() const {
	return !(channel_enable_ & 0x1f);
}

void SCC::get_samples(std::size_t number_of_samples, std::int16_t *target) {
	if(is_zero_level()) {
		std::memset(target, 0, sizeof(std::int16_t) * number_of_samples);
		return;
	}

	std::size_t c = 0;
	while((master_divider_&7) && c < number_of_samples) {
		target[c] = transient_output_level_;
		master_divider_++;
		c++;
	}

	while(c < number_of_samples) {
		for(int channel = 0; channel < 5; ++channel) {
			if(channels_[channel].tone_counter) channels_[channel].tone_counter--;
			else {
				channels_[channel].offset = (channels_[channel].offset + 1) & 0x1f;
				channels_[channel].tone_counter = channels_[channel].period;
			}
		}

		evaluate_output_volume();

		for(int ic = 0; ic < 8 && c < number_of_samples; ++ic) {
			target[c] = transient_output_level_;
			c++;
			master_divider_++;
		}
	}
}

void SCC::write(uint16_t address, uint8_t value) {
	address &= 0xff;
	if(address < 0x80) ram_[address] = value;

	task_queue_.enqueue([this, address, value] {
		// Check for a write into waveform memory.
		if(address < 0x80) {
			waves_[address >> 5].samples[address & 0x1f] = value;
		} else switch(address) {
			default: break;

			case 0x80: case 0x82: case 0x84: case 0x86: case 0x88: {
				int channel = (address - 0x80) >> 1;
				channels_[channel].period = (channels_[channel].period & ~0xff) | value;
			} break;

			case 0x81: case 0x83: case 0x85: case 0x87: case 0x89: {
				int channel = (address - 0x80) >> 1;
				channels_[channel].period = (channels_[channel].period & 0xff) | ((value & 0xf) << 8);
			} break;

			case 0x8a: case 0x8b: case 0x8c: case 0x8d: case 0x8e:
				channels_[address - 0x8a].amplitude = value & 0xf;
			break;

			case 0x8f:
				channel_enable_ = value;
			break;
		}

		evaluate_output_volume();
	});
}

void SCC::evaluate_output_volume() {
	transient_output_level_ =
		int16_t(
			((
				(channel_enable_ & 0x01) ? int8_t(waves_[0].samples[channels_[0].offset]) * channels_[0].amplitude : 0 +
				(channel_enable_ & 0x02) ? int8_t(waves_[1].samples[channels_[1].offset]) * channels_[1].amplitude : 0 +
				(channel_enable_ & 0x04) ? int8_t(waves_[2].samples[channels_[2].offset]) * channels_[2].amplitude : 0 +
				(channel_enable_ & 0x08) ? int8_t(waves_[3].samples[channels_[3].offset]) * channels_[3].amplitude : 0 +
				(channel_enable_ & 0x10) ? int8_t(waves_[3].samples[channels_[4].offset]) * channels_[4].amplitude : 0
			) * master_volume_) / (255*15*5)
			// Five channels, each with 8-bit samples and 4-bit volumes implies a natural range of 0 to 255*15*5.
		);
}

void SCC::set_sample_volume_range(std::int16_t range) {
	master_volume_ = range;
	evaluate_output_volume();
}

uint8_t SCC::read(uint16_t address) {
	address &= 0xff;
	if(address < 0x80) {
		return ram_[address];
	}
	return 0xff;
}


