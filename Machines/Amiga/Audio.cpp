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
#include <tuple>

using namespace Amiga;

Audio::Audio(Chipset &chipset, uint16_t *ram, size_t word_size, float output_rate) :
	DMADevice<4>(chipset, ram, word_size) {

	// Mark all buffers as available.
	for(auto &flag: buffer_available_) {
		flag.store(true, std::memory_order::memory_order_relaxed);
	}

	speaker_.set_input_rate(output_rate);
	speaker_.set_high_frequency_cutoff(7000.0f);
}

// MARK: - Exposed setters.

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

template <bool is_external> void Audio::set_data(int channel, uint16_t data) {
	assert(channel >= 0 && channel < 4);
	channels_[channel].wants_data = false;
	channels_[channel].data = data;

	// TODO: "the [PWM] counter is reset when ... AUDxDAT is written", but
	// does that just mean written by the CPU, or does it include DMA?
	// My guess is the former. But TODO.
	if constexpr (is_external) {
		channels_[channel].reset_output_phase();
	}
}

template void Audio::set_data<false>(int, uint16_t);
template void Audio::set_data<true>(int, uint16_t);

void Audio::set_channel_enables(uint16_t enables) {
	channels_[0].dma_enabled = enables & 1;
	channels_[1].dma_enabled = enables & 2;
	channels_[2].dma_enabled = enables & 4;
	channels_[3].dma_enabled = enables & 8;
}

void Audio::set_modulation_flags(uint16_t flags) {
	channels_[3].attach_period = flags & 0x80;
	channels_[2].attach_period = flags & 0x40;
	channels_[1].attach_period = flags & 0x20;
	channels_[0].attach_period = flags & 0x10;

	channels_[3].attach_volume = flags & 0x08;
	channels_[2].attach_volume = flags & 0x04;
	channels_[1].attach_volume = flags & 0x02;
	channels_[0].attach_volume = flags & 0x01;
}

void Audio::set_interrupt_requests(uint16_t requests) {
	channels_[0].interrupt_pending = requests & uint16_t(InterruptFlag::AudioChannel0);
	channels_[1].interrupt_pending = requests & uint16_t(InterruptFlag::AudioChannel1);
	channels_[2].interrupt_pending = requests & uint16_t(InterruptFlag::AudioChannel2);
	channels_[3].interrupt_pending = requests & uint16_t(InterruptFlag::AudioChannel3);
}

// MARK: - DMA and mixing.

bool Audio::advance_dma(int channel) {
	if(!channels_[channel].wants_data) {
		return false;
	}

	set_data<false>(channel, ram_[channels_[channel].data_address & ram_mask_]);
	++channels_[channel].data_address;

	if(channels_[channel].should_reload_address) {
		channels_[channel].data_address = pointer_[size_t(channel)];
		channels_[channel].should_reload_address = false;
	}

	return true;
}

void Audio::output() {
	constexpr InterruptFlag interrupts[] = {
		InterruptFlag::AudioChannel0,
		InterruptFlag::AudioChannel1,
		InterruptFlag::AudioChannel2,
		InterruptFlag::AudioChannel3,
	};
	Channel *const modulands[] = {
		&channels_[1],
		&channels_[2],
		&channels_[3],
		nullptr,
	};

	for(int c = 0; c < 4; c++) {
		if(channels_[c].output(modulands[c])) {
			posit_interrupt(interrupts[c]);
		}
	}

	// Spin until the next buffer is available if just entering it for the first time.
	// Contention here should be essentially non-existent.
	if(!sample_pointer_) {
		while(!buffer_available_[buffer_pointer_].load(std::memory_order::memory_order_relaxed));
	}

	// Left.
	static_assert(std::tuple_size<AudioBuffer>::value % 2 == 0);
	buffer_[buffer_pointer_][sample_pointer_] = int16_t(
		(
			channels_[1].output_level * channels_[1].output_enabled +
			channels_[2].output_level * channels_[2].output_enabled
		) << 7
	);

	// Right.
	buffer_[buffer_pointer_][sample_pointer_ + 1] = int16_t(
		(
			channels_[0].output_level * channels_[0].output_enabled +
			channels_[3].output_level * channels_[3].output_enabled
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

// MARK: - Per-channel logic.

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
			action: percntrld, AUDxIR, volcntrld, pbudld1

		-> State::WaitingForDummyDMA	(001)
			if: AUDxON
			action: percntrld, AUDxDR, lencntrld, dmasen*


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
				1. volcntrld, percntrld, pbufld1
				2. if napnav, then AUDxDR



	010 State::PlayingHigh

		-> State::PlayingHigh			(010)
			if: N/A
			action: percount, and penhi

		-> State::PlayingLow			(011)
			if: perfin
			action:
				1. if AUDxAP, then pbufld2
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
			if: perfin and not (AUDxON or not AUDxIP)
			action: None

		-> State::PlayingHigh			(010)
			if: perfin and (AUDxON or not AUDxIP)
			action:
				1. pbufld
				2. percntrld
				3. if napnav and AUDxON, then AUDxDR
				4. if napnav and AUDxON and intreq2, AUDxIR
				5. if napnav and not AUDxON, AUDxIR
				6. if lenfin and AUDxON and AUDxDAT, then lencntrld
				7. if (not lenfin) and AUDxON and AUDxDAT, then lencount
				8. if lenfin and AUDxON and AUDxDAT, then intreq2

				[note that 6-8 are shared with the High -> Low transition]



	Definitions:

		AUDxON		DMA on "x" indicates channel number (signal from DMACON).

		AUDxIP		Audio interrupt pending (input to channel from interrupt circuitry).

		AUDxIR		Audio interrupt request (output from channel to interrupt circuitry).

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
// Non-action fallback transition and setter, plus specialised begin_state declarations.
//

template <Audio::Channel::State end> void Audio::Channel::begin_state(Channel *) {
	state = end;
}
template <> void Audio::Channel::begin_state<Audio::Channel::State::PlayingHigh>(Channel *);
template <> void Audio::Channel::begin_state<Audio::Channel::State::PlayingLow>(Channel *);

template <
	Audio::Channel::State begin,
	Audio::Channel::State end> bool Audio::Channel::transit(Channel *moduland) {
	begin_state<end>(moduland);
	return false;
}

//
//	Audio::Channel::State::Disabled
//

template <> bool Audio::Channel::transit<
	Audio::Channel::State::Disabled,
	Audio::Channel::State::PlayingHigh>(Channel *moduland) {
	begin_state<State::PlayingHigh>(moduland);

	// percntrld
	period_counter = period;

	// [AUDxIR]: see return result.

	// volcntrld
	volume_latch = volume;
	reset_output_phase();

	// pbufld1
	data_latch = data;
	wants_data = true;

	// AUDxIR.
	return true;
}

template <> bool Audio::Channel::transit<
	Audio::Channel::State::Disabled,
	Audio::Channel::State::WaitingForDummyDMA>(Channel *moduland) {
	begin_state<State::WaitingForDummyDMA>(moduland);

	// percntrld
	period_counter = period;

	// AUDxDR
	wants_data = true;

	// lencntrld
	length_counter = length;

	// dmasen / AUDxDSR
	should_reload_address = true;

	return false;
}

template <> bool Audio::Channel::output<Audio::Channel::State::Disabled>(Channel *moduland) {
	// if AUDDAT, and not AUDxON, and not AUDxIP.
	if(!wants_data && !dma_enabled && !interrupt_pending) {
		return transit<State::Disabled, State::PlayingHigh>(moduland);
	}

	// if AUDxON.
	if(dma_enabled) {
		return transit<State::Disabled, State::WaitingForDummyDMA>(moduland);
	}

	return false;
}

//
//	Audio::Channel::State::WaitingForDummyDMA
//

template <> bool Audio::Channel::transit<
	Audio::Channel::State::WaitingForDummyDMA,
	Audio::Channel::State::WaitingForDMA>(Channel *moduland) {
	begin_state<State::WaitingForDMA>(moduland);

	// AUDxDR
	wants_data = true;

	// if not lenfin, then lencount
	if(length != 1) {
		-- length_counter;
	}

	// AUDxIR
	return true;
}

template <> bool Audio::Channel::output<Audio::Channel::State::WaitingForDummyDMA>(Channel *moduland) {
	// if not AUDxON
	if(!dma_enabled) {
		return transit<State::WaitingForDummyDMA, State::Disabled>(moduland);
	}

	// if AUDxON and AUDxDAT
	if(dma_enabled && !wants_data) {
		return transit<State::WaitingForDummyDMA, State::WaitingForDMA>(moduland);
	}

	return false;
}

//
//	Audio::Channel::State::WaitingForDMA
//

template <> bool Audio::Channel::transit<
	Audio::Channel::State::WaitingForDMA,
	Audio::Channel::State::PlayingHigh>(Channel *moduland) {
	begin_state<State::PlayingHigh>(moduland);

	// volcntrld
	volume_latch = volume;
	reset_output_phase();

	// percntrld
	period_counter = period;

	// pbufld1
	data_latch = data;

	// if napnav
	if(attach_volume || !(attach_volume || attach_period)) {
		// AUDxDR
		wants_data = true;
	}

	return false;
}

template <> bool Audio::Channel::output<Audio::Channel::State::WaitingForDMA>(Channel *moduland) {
	// if: not AUDxON
	if(!dma_enabled) {
		return transit<State::WaitingForDummyDMA, State::Disabled>(moduland);
	}

	// if: AUDxON, and AUDxDAT
	if(dma_enabled && !wants_data) {
		return transit<State::WaitingForDummyDMA, State::PlayingHigh>(moduland);
	}

	return false;
}

//
//	Audio::Channel::State::PlayingHigh
//

void Audio::Channel::decrement_length() {
	// if lenfin and AUDxON and AUDxDAT, then lencntrld
	// if (not lenfin) and AUDxON and AUDxDAT, then lencount
	// if lenfin and AUDxON and AUDxDAT, then intreq2
	if(dma_enabled && !wants_data) {
		-- length_counter;

		if(!length_counter) {
			length_counter = length;
			will_request_interrupt = true;
			should_reload_address = true;	// This feels logical to me; it's a bit
											// of a stab in the dark though.
		}
	}
}

template <> bool Audio::Channel::transit<
	Audio::Channel::State::PlayingHigh,
	Audio::Channel::State::PlayingLow>(Channel *moduland) {
	begin_state<State::PlayingLow>(moduland);

	bool wants_interrupt = false;

	// if AUDxAP
	if(attach_period) {
		// pbufld2
		data_latch = data;

		// [if AUDxAP] and AUDxON
		if(dma_enabled) {
			// AUDxDR
			wants_data = true;

			// [if AUDxAP and AUDxON] and intreq2
			if(will_request_interrupt) {
				will_request_interrupt = false;

				// AUDxIR
				wants_interrupt = true;
			}
		} else {
			// i.e. if AUDxAP and AUDxON, then AUDxIR
			wants_interrupt = true;
		}
	}

	// percntrld
	period_counter = period;

	decrement_length();

	return wants_interrupt;
}

template <> void Audio::Channel::begin_state<Audio::Channel::State::PlayingHigh>(Channel *) {
	state = Audio::Channel::State::PlayingHigh;

	// penhi.
	output_level = int8_t(data_latch >> 8);
}

template <> bool Audio::Channel::output<Audio::Channel::State::PlayingHigh>(Channel *moduland) {
	// This is a reasonable guess as to the exit condition for this node;
	// Commodore doesn't document.
	if(period_counter == 1) {
		return transit<State::PlayingHigh, State::PlayingLow>(moduland);
	}

	// percount.
	-- period_counter;

	return false;
}

//
//	Audio::Channel::State::PlayingLow
//

template <> bool Audio::Channel::transit<
	Audio::Channel::State::PlayingLow,
	Audio::Channel::State::Disabled>(Channel *moduland) {
	begin_state<State::Disabled>(moduland);

	// Clear the slightly nebulous 'if intreq2 occurred' state.
	will_request_interrupt = false;

	return false;
}

template <> bool Audio::Channel::transit<
	Audio::Channel::State::PlayingLow,
	Audio::Channel::State::PlayingHigh>(Channel *moduland) {
	begin_state<State::PlayingHigh>(moduland);

	bool wants_interrupt = false;

	// volcntrld
	volume_latch = volume;
	reset_output_phase();	// Is this correct?

	// percntrld
	period_counter = period;

	// pbufld1
	data_latch = data;

	// if napnav
	if(attach_volume || !(attach_volume || attach_period)) {
		// [if napnav] and AUDxON
		if(dma_enabled) {
			// AUDxDR
			wants_data = true;

			// [if napnav and AUDxON] and intreq2
			if(will_request_interrupt) {
				will_request_interrupt = false;
				wants_interrupt = true;
			}
		} else {
			// AUDxIR
			wants_interrupt = true;
		}
	}

	decrement_length();

	return wants_interrupt;
}

template <> void Audio::Channel::begin_state<Audio::Channel::State::PlayingLow>(Channel *) {
	state = Audio::Channel::State::PlayingLow;

	// Output low byte.
	output_level = int8_t(data_latch & 0xff);
}

template <> bool Audio::Channel::output<Audio::Channel::State::PlayingLow>(Channel *moduland) {
	-- period_counter;

	if(!period_counter) {
		const bool dma_or_no_interrupt = dma_enabled || !interrupt_pending;
		if(dma_or_no_interrupt) {
			return transit<State::PlayingLow, State::PlayingHigh>(moduland);
		} else {
			return transit<State::PlayingLow, State::Disabled>(moduland);
		}
	}

	return false;
}

//
// Dispatcher
//

bool Audio::Channel::output(Channel *moduland) {
	// Update pulse-width modulation.
	output_phase = output_phase + 1;
	if(output_phase == 64) {
		reset_output_phase();
	} else {
		output_enabled &= output_phase != volume_latch;
	}

	switch(state) {
		case State::Disabled:			return output<State::Disabled>(moduland);
		case State::WaitingForDummyDMA:	return output<State::WaitingForDummyDMA>(moduland);
		case State::WaitingForDMA:		return output<State::WaitingForDMA>(moduland);
		case State::PlayingHigh:		return output<State::PlayingHigh>(moduland);
		case State::PlayingLow:			return output<State::PlayingLow>(moduland);

		default:
			assert(false);
		break;
	}

	return false;
}
