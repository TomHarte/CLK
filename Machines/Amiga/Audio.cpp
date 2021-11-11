//
//  Audio.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 09/11/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "Audio.hpp"
#include <cassert>

using namespace Amiga;

bool Audio::advance(int channel) {
	if(channels_[channel].samples_remaining || !channels_[channel].length) {
		return false;
	}

	set_data(channel, ram_[pointer_[size_t(channel)]]);
	++pointer_[size_t(channel)];
	--channels_[channel].length;

	return false;
}

void Audio::set_length(int channel, uint16_t length) {
	assert(channel >= 0 && channel < 4);
	channels_[channel].length = length;
}

void Audio::set_period(int channel, uint16_t period) {
	assert(channel >= 0 && channel < 4);
	channels_[channel].period = period;
}

void Audio::set_volume(int channel, uint16_t volume) {
	assert(channel >= 0 && channel < 4);
	channels_[channel].volume = (volume & 0x40) ? 64 : (volume & 0x3f);
}

void Audio::set_data(int channel, uint16_t data) {
	assert(channel >= 0 && channel < 4);
	if(!channels_[channel].samples_remaining) {
		channels_[channel].period_counter = channels_[channel].period;
	}
	channels_[channel].samples_remaining = 2;
	channels_[channel].data = data;
}

void Audio::set_channel_enables(uint16_t enables) {
	channels_[0].dma_enabled = enables & 1;
	channels_[1].dma_enabled = enables & 2;
	channels_[2].dma_enabled = enables & 4;
	channels_[3].dma_enabled = enables & 8;
}

void Audio::set_modulation_flags(uint16_t) {
}

void Audio::run_for(HalfCycles) {
	// TODO:
	//
	// Check whether any channel's period counter is exhausted and, if
	// so, attempt to consume another sample. If there are no more samples
	// and length is 0, trigger an interrupt.
}
