//
//  Audio.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 09/11/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "Audio.hpp"

#include "Flags.hpp"

#define LOG_PREFIX "[Audio] "
#include "../../Outputs/Log.hpp"

#include <cassert>

using namespace Amiga;

bool Audio::advance(int channel) {
	if(channels_[channel].has_data || !channels_[channel].length) {
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
	if(!channels_[channel].has_data) {
		channels_[channel].period_counter = channels_[channel].period;
	}
	channels_[channel].has_data = true;
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

void Audio::set_interrupt_requests(uint16_t requests) {
	channels_[0].interrupt_pending = requests & uint16_t(InterruptFlag::AudioChannel0);
	channels_[1].interrupt_pending = requests & uint16_t(InterruptFlag::AudioChannel1);
	channels_[2].interrupt_pending = requests & uint16_t(InterruptFlag::AudioChannel2);
	channels_[3].interrupt_pending = requests & uint16_t(InterruptFlag::AudioChannel3);
}

void Audio::run_for([[maybe_unused]] Cycles duration) {
	// TODO:
	//
	// Check whether any channel's period counter is exhausted and, if
	// so, attempt to consume another sample. If there are no more samples
	// and length is 0, trigger an interrupt.

	using State = Channel::State;
	for(int c = 0; c < 4; c++) {
		switch(channels_[c].state) {
			case State::Disabled:
				if(channels_[c].has_data && !channels_[c].dma_enabled && !channels_[c].interrupt_pending) {
					channels_[c].state = Channel::State::PlayingHigh;
					// TODO: [volcntrld, percntrld, pbufldl, AUDxID]
				}
			break;

			default: break;
		}
	}
}
