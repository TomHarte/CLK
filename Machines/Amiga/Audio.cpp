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
	// The following attempts to reproduce the audio state diagram provided in
	// Commodore's Hardware Reference Manual.
	//
	// See big comment at the foot of this file.

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

			// This is a reasonable guess as to the exit condition for this node;
			// Commodore doesn't document.
			if(period_counter == 1) {
				state = State::PlayingLow;

				// TODO: if attach period, reload output buffer.
			}
		break;

		default: break;
	}

	return false;
}

/*
	Big spiel on the state machine implemented above:

	Commodore's Hardware Rerefence Manual provides the audio subsystem's state
	machine, so I've just tried to reimplement it verbatim. It's depicted
	diagrammatically in the original source as a finite state automata, the
	below is my attempt to translate that into text.


	000 State::Disabled:

		-> State::Disabled
			if: N/A
			action: percentrld

		-> State::PlayingHigh
			if: AUDDAT, and not AUDxON, and not AUDxIP
			action: volcntrld, percentrld, pbudld1, AUDxIR

		-> State::WaitingForDummyDMA
			if: AUDxON
			action: lenctrld, AUDxDR, dmasen, percntrld



	001 State::WaitingForDummyDMA:

		-> State::WaitingForDummyDMA
			if: N/A
			action: None

		-> State::Disabled
			if: not AUDxON
			action: None

		-> State::WaitingForDMA
			if: AUDxON, and AUDxDAT
			action:
				1. AUDxIR
				2. if not lenfin, then lencount



	101 State::WaitingForDMA:

		-> State::WaitingForDMA:
			if: N/A
			action: None

		-> State:Disabled
			if: not AUDxON
			action: None

		-> State::PlayingHigh
			if: AUDxON, and AUDxDAT
			action:
				1. volcntrld, percentrld, pbufid1
				2. if napnav, AUDxDR



	010 State::PlayingHigh

		-> State::PlayingHigh
			if: N/A
			action: percount, and penhi

		-> State::PlayingLow
			if: [unspecified, presumably 'not percount']
			action:
				1. if AUDxAP then pubfid2
				2. if AUDxAP and AUDxON. then AUDxDR
				3. percentrld
				4. if intreq2 and AUDxON and AUDxAP, then AUDxIR
				5. if AUDxAP and AUDxON, then AUDxIR
				6. if lenfin and AUDxON and AUDxDAT, then lenctrld
				7. if (not lenfin) and AUDxON and AUDxDAT, then lencount
				8. if lenfin and AUDxON and AUDxDAT, then intreq2

				[note that 6–8 are shared with the Low -> High transition]



	011 State::PlayingLow

		-> State::PlayingLow
			if: N/A
			action: percount, and not penhi

		-> State::Disabled
			if: perfin and not (AUDxON and not AUDxIP)
			action: None

		-> State::PlayingHigh
			if: perfin and AUDxON and not AUDxIP
			action:
				1. pbufld
				2. percntrld
				3. if AUDxON and napnav, then AUDxDR
				4. if lenfin and AUDxON and AUDxDAT, then lenctrld
				5. if (not lenfin) and AUDxON and AUDxDAT, then lencount
				6. if lenfin and AUDxON and AUDxDAT, then intreq2

				[note that 4–6 are shared with the High -> Low transition]



	Definitions:

		AUDxON		DMA on "x" indicates channel number (signal from  DMACON).

		AUDxIP		Audio interrupt  pending (input to channel from interrupt circuitry).

		AUDxIR		Audio interrupt  request (output from channel to interrupt circuitry)

		intreq1		Interrupt request that combines with intreq2 to form AUDxIR.

		intreq2		Prepare for interrupt request. Request comes out after the
					next 011->010 transition in normal operation.

		AUDxDAT		Audio data load signal. Loads 16 bits of data to audio channel.

		AUDxDR		Audio DMA request to Agnus for one word of data.

		AUDxDSR		Audio DMA request to Agnus to reset pointer to start of block.

		dmasen		Restart request enable.

		percntrld	Reload period counter from back-up latch typically written
					by processor with  AUDxPER  (can also be written by attach mode).

		percount    Count period counter down one latch.

		perfin      Period counter finished (value = 1).

		lencntrld   Reload length counter from back-up latch.

		lencount    Count length counter down one notch.

		lenfin      Length counter finished (value = 1).

		volcntrld   Reload volume counter from back-up latch.

		pbufld1     Load output buffer from holding latch written to by AUDxDAT.

		pbufld2     Like pbufld1, but only during 010->011 with attach period.

		AUDxAV      Attach volume. Send data to volume latch of next channel
					instead of to D->A converter.

		AUDxAP      Attach period. Send data to period latch of next channel
					instead of to the D->A converter.

		penhi       Enable the high 8 bits of data to go to the D->A converter.

		napnav      /AUDxAV * /AUDxAP + AUDxAV -- no attach stuff or else attach
					volume. Condition for normal DMA and interrupt requests.
*/
