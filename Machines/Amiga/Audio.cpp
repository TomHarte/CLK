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

bool Audio::advance_dma(int channel) {
	switch(channels_[channel].state) {
		case Channel::State::WaitingForDMA:
			set_data(channel, ram_[pointer_[size_t(channel)]]);
		return true;
		case Channel::State::WaitingForDummyDMA:
			channels_[channel].has_data = true;
		return true;
		default:
		return false;
	}
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

void Audio::output() {
	// TODO:
	//
	// Check whether any channel's period counter is exhausted and, if
	// so, attempt to consume another sample. If there are no more samples
	// and length is 0, trigger an interrupt.

	constexpr InterruptFlag interrupts[] = {
		InterruptFlag::AudioChannel0,
		InterruptFlag::AudioChannel1,
		InterruptFlag::AudioChannel2,
		InterruptFlag::AudioChannel3,
	};

	for(int c = 0; c < 4; c++) {
		if(channels_[c].output()) {
			posit_interrupt(interrupts[c]);
		}
	}
}

bool Audio::Channel::output() {
	switch(state) {
		case State::Disabled:
			// Test for top loop of Commodore's state diagram,
			// which permits CPU-driven audio output.
			if(has_data && !dma_enabled && !interrupt_pending) {
				state = State::PlayingHigh;

				data_latch = data;			// i.e. pbufld1
				has_data = false;
				period_counter = period;	// i.e. percntrld
				// TODO: volcntrld (see above).

				// Request an interrupt.
				return true;
			}

			// Test for DMA-style transition.
			if(dma_enabled) {
				state = State::WaitingForDummyDMA;

				period_counter = period;	// i.e. percntrld
				length_counter = length;	// i.e. lenctrld

				break;
			}
		break;

		case State::WaitingForDummyDMA:
			if(!dma_enabled) {
				state = State::Disabled;
				break;
			}

			if(dma_enabled && has_data) {
				has_data = false;
				state = State::WaitingForDMA;
				if(length == 1) {
					length_counter = length;
					return true;
				}
				break;
			}
		break;

		case State::WaitingForDMA:
			if(!dma_enabled) {
				state = State::Disabled;
				break;
			}

			if(dma_enabled && has_data) {
				data_latch = data;
				has_data = false;
				period_counter = period;	// i.e. percntrld

				state = State::PlayingHigh;
				break;
			}
		break;

		case State::PlayingHigh:
			// TODO: penhi (i.e. output high byte).
			-- period_counter;
			if(!period_counter) {

			}
		break;

		default: break;
	}

	return false;
}
