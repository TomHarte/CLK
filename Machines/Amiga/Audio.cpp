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

Audio::Audio(Chipset &chipset, uint16_t *ram, size_t word_size, float output_rate) :
	DMADevice<4>(chipset, ram, word_size) {

	// Mark all buffers as available.
	for(auto &flag: buffer_available_) {
		flag.store(true, std::memory_order::memory_order_relaxed);
	}

	speaker_.set_input_rate(output_rate);
}

bool Audio::advance_dma(int channel) {
	if(!channels_[channel].wants_data) {
		return false;
	}

	set_data(channel, ram_[pointer_[size_t(channel)]]);
	if(channels_[channel].state != Channel::State::WaitingForDummyDMA) {
		++pointer_[size_t(channel)];
	}

	return true;
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
	channels_[channel].wants_data = false;
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

	// TEMPORARY: just fill the audio buffer with silence.
	if(!sample_pointer_) {
		while(!buffer_available_[buffer_pointer_].load(std::memory_order::memory_order_relaxed));
	}

	buffer_[buffer_pointer_][sample_pointer_] = int16_t(
		(
			int8_t(channels_[0].output_level) * channels_[0].output_enabled +
			int8_t(channels_[2].output_level) * channels_[2].output_enabled
		) << 7
	);
	buffer_[buffer_pointer_][sample_pointer_+1] = int16_t(
		(
			int8_t(channels_[1].output_level) * channels_[1].output_enabled +
			int8_t(channels_[3].output_level) * channels_[3].output_enabled
		) << 7
	);
	sample_pointer_ += 2;

	if(sample_pointer_ == buffer_[buffer_pointer_].size()) {
		const auto &buffer = buffer_[buffer_pointer_];
		auto &flag = buffer_available_[buffer_pointer_];

		flag.store(false, std::memory_order::memory_order_release);
		queue_.enqueue([this, &buffer, &flag] {
			speaker_.push(buffer.data(), buffer.size() >> 1);
			flag.store(true, std::memory_order::memory_order_relaxed);
		});

		buffer_pointer_ = (buffer_pointer_ + 1) % BufferCount;
		sample_pointer_ = 0;
	}
}

/*
	Big spiel on the state machine:

	Commodore's Hardware Rerefence Manual provides the audio subsystem's state
	machine, so I've just tried to reimplement it verbatim. It's depicted
	diagrammatically in the original source as a finite state automata, the
	below is my attempt to translate that into text.


	000 State::Disabled:

		-> State::Disabled				(000)
			if: N/A
			action: percntrld

		-> State::PlayingHigh			(010)
			if: AUDDAT, and not AUDxON, and not AUDxIP
			action: volcntrld, percntrld, pbudld1, AUDxIR

		-> State::WaitingForDummyDMA	(001)
			if: AUDxON
			action: lencntrld, AUDxDR, dmasen*, percntrld


		*	NOTE: except for this case, dmasen is true only when
			LENFIN = 1. Also, AUDxDSR = (AUDxDR and dmasen).



	001 State::WaitingForDummyDMA:

		-> State::WaitingForDummyDMA	(001)
			if: N/A
			action: None

		-> State::Disabled				(000)
			if: not AUDxON
			action: None

		-> State::WaitingForDMA			(101)
			if: AUDxON, and AUDxDAT
			action:
				1. AUDxIR
				2. if not lenfin, then lencount



	101 State::WaitingForDMA:

		-> State::WaitingForDMA			(101)
			if: N/A
			action: None

		-> State:Disabled				(000)
			if: not AUDxON
			action: None

		-> State::PlayingHigh			(010)
			if: AUDxON, and AUDxDAT
			action:
				1. volcntrld, percntrld, pbufid1
				2. if napnav, then AUDxDR



	010 State::PlayingHigh

		-> State::PlayingHigh			(010)
			if: N/A
			action: percount, and penhi

		-> State::PlayingLow			(011)
			if: perfin
			action:
				1. if AUDxAP, then pubfid2
				2. if AUDxAP and AUDxON, then AUDxDR
				3. percntrld
				4. if intreq2 and AUDxON and AUDxAP, then AUDxIR
				5. if AUDxAP and AUDxON, then AUDxIR
				6. if lenfin and AUDxON and AUDxDAT, then lencntrld
				7. if (not lenfin) and AUDxON and AUDxDAT, then lencount
				8. if lenfin and AUDxON and AUDxDAT, then intreq2

				[note that 6–8 are shared with the Low -> High transition]



	011 State::PlayingLow

		-> State::PlayingLow			(011)
			if: N/A
			action: percount, and not penhi

		-> State::Disabled				(000)
			if: perfin and not (AUDxON and not AUDxIP)
			action: None

		-> State::PlayingHigh			(010)
			if: perfin and AUDxON and not AUDxIP
			action:
				1. pbufld
				2. percntrld
				3. if AUDxON and napnav, then AUDxDR
				4. if intreq2 and napnav and AUDxON, AUDxIR
				5. if napnav and not AUDxON, AUDxIR
				6. if lenfin and AUDxON and AUDxDAT, then lencntrld
				7. if (not lenfin) and AUDxON and AUDxDAT, then lencount
				8. if lenfin and AUDxON and AUDxDAT, then intreq2

				[note that 6-8 are shared with the High -> Low transition]



	Definitions:

		AUDxON		DMA on "x" indicates channel number (signal from DMACON).

		AUDxIP		Audio interrupt pending (input to channel from interrupt circuitry).

		AUDxIR		Audio interrupt request (output from channel to interrupt circuitry)

		intreq1		Interrupt request that combines with intreq2 to form AUDxIR.

		intreq2		Prepare for interrupt request. Request comes out after the
					next 011->010 transition in normal operation.

		AUDxDAT		Audio data load signal. Loads 16 bits of data to audio channel.

		AUDxDR		Audio DMA request to Agnus for one word of data.

		AUDxDSR		Audio DMA request to Agnus to reset pointer to start of block.

		dmasen		Restart request enable.

		percntrld	Reload period counter from back-up latch typically written
					by processor with AUDxPER (can also be written by attach mode).

		percount	Count period counter down one latch.

		perfin		Period counter finished (value = 1).

		lencntrld	Reload length counter from back-up latch.

		lencount	Count length counter down one notch.

		lenfin		Length counter finished (value = 1).

		volcntrld	Reload volume counter from back-up latch.

		pbufld1		Load output buffer from holding latch written to by AUDxDAT.

		pbufld2		Like pbufld1, but only during 010->011 with attach period.

		AUDxAV		Attach volume. Send data to volume latch of next channel
					instead of to D->A converter.

		AUDxAP		Attach period. Send data to period latch of next channel
					instead of to the D->A converter.

		penhi		Enable the high 8 bits of data to go to the D->A converter.

		napnav		/AUDxAV * /AUDxAP + AUDxAV -- no attach stuff or else attach
					volume. Condition for normal DMA and interrupt requests.
*/


//
// Non-action fallback transition.
//

template <
	Audio::Channel::State begin,
	Audio::Channel::State end> bool Audio::Channel::transit() {
	state = end;
	return false;
}

//
//	Audio::Channel::State::Disabled
//

template <> bool Audio::Channel::transit<
	Audio::Channel::State::Disabled,
	Audio::Channel::State::WaitingForDummyDMA>() {
	state = State::WaitingForDummyDMA;

	period_counter = period;	// i.e. percntrld
	length_counter = length;	// i.e. lencntrld

	return false;
}

template <> bool Audio::Channel::transit<
	Audio::Channel::State::Disabled,
	Audio::Channel::State::PlayingHigh>() {
	state = State::PlayingHigh;

	data_latch = data;			// i.e. pbufld1
	wants_data = true;
	period_counter = period;	// i.e. percntrld
	// TODO: volcntrld (see above).

	// Request an interrupt.
	return true;
}

template <> bool Audio::Channel::output<Audio::Channel::State::Disabled>() {
	if(!wants_data && !dma_enabled && !interrupt_pending) {
		return transit<State::Disabled, State::PlayingHigh>();
	}

	// Test for DMA-style transition.
	if(dma_enabled) {
		return transit<State::Disabled, State::WaitingForDummyDMA>();
	}

	return false;
}

//
//	Audio::Channel::State::WaitingForDummyDMA
//

template <> bool Audio::Channel::transit<
	Audio::Channel::State::WaitingForDummyDMA,
	Audio::Channel::State::WaitingForDMA>() {
	state = State::WaitingForDMA;

	wants_data = true;
	if(length == 1) {
		length_counter = length;
		return true;
	}

	return false;
}

template <> bool Audio::Channel::output<Audio::Channel::State::WaitingForDummyDMA>() {
	if(!dma_enabled) {
		return transit<State::WaitingForDummyDMA, State::Disabled>();
	}

	if(dma_enabled && !wants_data) {
		return transit<State::WaitingForDummyDMA, State::WaitingForDMA>();
	}

	return false;
}

//
//	Audio::Channel::State::WaitingForDMA
//

template <> bool Audio::Channel::transit<
	Audio::Channel::State::WaitingForDMA,
	Audio::Channel::State::PlayingHigh>() {
	state = State::PlayingHigh;

	data_latch = data;
	wants_data = true;
	period_counter = period;	// i.e. percntrld

	return false;
}

template <> bool Audio::Channel::output<Audio::Channel::State::WaitingForDMA>() {
	if(!dma_enabled) {
		return transit<State::WaitingForDummyDMA, State::Disabled>();
	}

	if(dma_enabled && !wants_data) {
		return transit<State::WaitingForDummyDMA, State::PlayingHigh>();
	}

	return false;
}

//
//	Audio::Channel::State::PlayingHigh
//

template <> bool Audio::Channel::transit<
	Audio::Channel::State::PlayingHigh,
	Audio::Channel::State::PlayingLow>() {
	state = State::PlayingLow;

	// TODO: if AUDxAP, then pubfid2
	// TODO: if AUDxAP and AUDxON, then AUDxDR

	period_counter = period;	// i.e. percntrld

	// TODO: if intreq2 and AUDxON and AUDxAP, then AUDxIR
	// TODO: if AUDxAP and AUDxON, then AUDxIR

	// if lenfin and AUDxON and AUDxDAT, then lencntrld
	// if (not lenfin) and AUDxON and AUDxDAT, then lencount
	// if lenfin and AUDxON and AUDxDAT, then intreq2
	if(dma_enabled && !wants_data) {
		--length_counter;
		if(!length_counter) {
			length_counter = length;
			will_request_interrupt = true;
		}
	}

	return false;
}

template <> bool Audio::Channel::output<Audio::Channel::State::PlayingHigh>() {
	-- period_counter;

	// This is a reasonable guess as to the exit condition for this node;
	// Commodore doesn't document.
	if(!period_counter) {
		return transit<State::PlayingHigh, State::PlayingLow>();
	}

	// Output high byte.
	output_level = data_latch >> 8;

	return false;
}

//
//	Audio::Channel::State::PlayingLow
//

template <> bool Audio::Channel::transit<
	Audio::Channel::State::PlayingLow,
	Audio::Channel::State::PlayingHigh>() {
	state = State::PlayingHigh;

	// TODO: include napnav in tests

	if(!dma_enabled) {
		return true;
	} else {
		data_latch = data;			// i.e. pbufld2
		wants_data = true;			// AUDxDR
	}

	if(dma_enabled && will_request_interrupt) {
		will_request_interrupt = false;
		return true;
	}

	return false;
}

template <> bool Audio::Channel::output<Audio::Channel::State::PlayingLow>() {
	-- period_counter;

	const bool dma_and_not_done = dma_enabled && !interrupt_pending;

	if(!period_counter && !dma_and_not_done) {
		return transit<State::PlayingLow, State::Disabled>();
	}

	if(!period_counter && dma_and_not_done) {
		return transit<State::PlayingLow, State::PlayingHigh>();
	}

	// Output low byte.
	output_level = data_latch & 0xff;

	return false;
}

//
// Dispatcher
//

bool Audio::Channel::output() {
	// Update pulse-width modulation.
	output_phase = (output_phase + 1) & 63;
	output_enabled |= !output_phase;
	output_enabled &= output_phase != volume;

	switch(state) {
		case State::Disabled:			return output<State::Disabled>();
		case State::WaitingForDummyDMA:	return output<State::WaitingForDummyDMA>();
		case State::WaitingForDMA:		return output<State::WaitingForDMA>();
		case State::PlayingHigh:		return output<State::PlayingHigh>();
		case State::PlayingLow:			return output<State::PlayingLow>();

		default:
			assert(false);
		break;
	}

	return false;
}
