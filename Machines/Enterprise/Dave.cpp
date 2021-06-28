//
//  Dave.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 22/06/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "Dave.hpp"

using namespace Enterprise::Dave;

// MARK: - Audio generator

Audio::Audio(Concurrency::DeferringAsyncTaskQueue &audio_queue) :
	audio_queue_(audio_queue) {}

void Audio::write(uint16_t address, uint8_t value) {
	address &= 0xf;
	audio_queue_.defer([address, value, this] {
		switch(address) {
			case 0:	case 2:	case 4:
				channels_[address >> 1].reload = (channels_[address >> 1].reload & 0xff00) | value;
			break;
			case 1:	case 3:	case 5:
				channels_[address >> 1].reload = uint16_t((channels_[address >> 1].reload & 0x00ff) | ((value & 0xf) << 8));
				channels_[address >> 1].distortion = Channel::Distortion((value >> 4)&3);
				channels_[address >> 1].high_pass = value & 0x40;
				channels_[address >> 1].ring_modulate = value & 0x80;
			break;
			case 6:
				noise_.frequency = Noise::Frequency(value&3);
				noise_.polynomial = Noise::Polynomial((value >> 2)&3);
				noise_.swap_polynomial = value & 0x10;
				noise_.low_pass = value & 0x20;
				noise_.high_pass = value & 0x40;
				noise_.ring_modulate = value & 0x80;
			break;

			case 7:
				channels_[0].sync = value & 0x01;
				channels_[1].sync = value & 0x02;
				channels_[2].sync = value & 0x04;
				use_direct_output_[0] = value & 0x08;
				use_direct_output_[1] = value & 0x10;
				// Interrupt bits are handled separately.
			break;

			case 8: case 9: case 10:
				channels_[address - 8].amplitude[0] = value & 0x3f;
			break;
			case 12: case 13: case 14:
				channels_[address - 12].amplitude[1] = value & 0x3f;
			break;
			case 11:	noise_.amplitude[0] = value & 0x3f;		break;
			case 15:	noise_.amplitude[1] = value & 0x3f;		break;
		}
	});
}

void Audio::set_sample_volume_range(int16_t range) {
	audio_queue_.defer([range, this] {
		volume_ = range / (63*4);
	});
}

void Audio::update_channel(int c) {
	if(channels_[c].sync) {
		channels_[c].count = channels_[c].reload;
		channels_[c].output <<= 1;
		return;
	}

	auto output = channels_[c].output & 1;
	channels_[c].output <<= 1;
	if(!channels_[c].count) {
		channels_[c].count = channels_[c].reload;

		if(channels_[c].distortion == Channel::Distortion::None)
			output ^= 1;
		else
			output = poly_state_[int(channels_[c].distortion)];

		if(channels_[c].high_pass && (channels_[(c+1)%3].output&3) == 2) {
			output = 0;
		}
		if(channels_[c].ring_modulate) {
			output = ~(output ^ channels_[(c+2)%3].output) & 1;
		}
	} else {
		--channels_[c].count;
	}

	channels_[c].output |= output;
}

void Audio::get_samples(std::size_t number_of_samples, int16_t *target) {
	for(size_t c = 0; c < number_of_samples; c++) {
		poly_state_[int(Channel::Distortion::FourBit)] = poly4_.next();
		poly_state_[int(Channel::Distortion::FiveBit)] = poly5_.next();
		poly_state_[int(Channel::Distortion::SevenBit)] = poly7_.next();
		if(noise_.swap_polynomial) {
			poly_state_[int(Channel::Distortion::SevenBit)] = poly_state_[int(Channel::Distortion::None)];
		}

		// Update tone channels.
		update_channel(0);
		update_channel(1);
		update_channel(2);

		// Update noise channel.

		// Step 1: decide whether there is a tick to apply.
		bool noise_tick = false;
		if(noise_.frequency == Noise::Frequency::DivideByFour) {
			if(!noise_.count) {
				noise_tick = true;
				noise_.count = 3;
			} else {
				--noise_.count;
			}
		} else {
			noise_tick = (channels_[int(noise_.frequency) - 1].output&3) == 2;
		}

		// Step 2: tick if necessary.
		if(noise_tick) {
			switch(noise_.polynomial) {
				case Noise::Polynomial::SeventeenBit:
					poly_state_[int(Channel::Distortion::None)] = uint8_t(poly17_.next());
				break;
				case Noise::Polynomial::FifteenBit:
					poly_state_[int(Channel::Distortion::None)] = uint8_t(poly15_.next());
				break;
				case Noise::Polynomial::ElevenBit:
					poly_state_[int(Channel::Distortion::None)] = uint8_t(poly11_.next());
				break;
				case Noise::Polynomial::NineBit:
					poly_state_[int(Channel::Distortion::None)] = uint8_t(poly9_.next());
				break;
			}

			noise_.output <<= 1;
			noise_.output |= poly_state_[int(Channel::Distortion::None)];

			// Low pass: sample channel 2 on downward transitions of the prima facie output.
			if(noise_.low_pass) {
				if((noise_.output & 3) == 2) {
					noise_.output = (noise_.output & ~1) | (channels_[2].output & 1);
				} else {
					noise_.output = (noise_.output & ~1) | (noise_.output & 1);
				}
			}
		}

		// Apply noise high-pass at the rate of the tone channels.
		if(noise_.high_pass && (channels_[0].output & 3) == 2) {
			noise_.output &= ~1;
		}

		// Update noise ring modulation, if any.
		if(noise_.ring_modulate) {
			noise_.final_output = !((noise_.output ^ channels_[1].output) & 1);
		} else {
			noise_.final_output = noise_.output & 1;
		}

		// I'm unclear on the details of the time division multiplexing so,
		// for now, just sum the outputs.
		target[(c << 1) + 0] =
			volume_ *
				(use_direct_output_[0] ?
					channels_[0].amplitude[0]
					: (
						channels_[0].amplitude[0] * (channels_[0].output & 1) +
						channels_[1].amplitude[0] * (channels_[1].output & 1) +
						channels_[2].amplitude[0] * (channels_[2].output & 1) +
						noise_.amplitude[0] * noise_.final_output
				));

		target[(c << 1) + 1] =
			volume_ *
				(use_direct_output_[1] ?
					channels_[0].amplitude[1]
					: (
						channels_[0].amplitude[1] * (channels_[0].output & 1) +
						channels_[1].amplitude[1] * (channels_[1].output & 1) +
						channels_[2].amplitude[1] * (channels_[2].output & 1) +
						noise_.amplitude[1] * noise_.final_output
				));
	}
}

// MARK: - Interrupt source

uint8_t TimedInterruptSource::get_new_interrupts() {
	const uint8_t result = interrupts_;
	interrupts_ = 0;
	return result;
}

void TimedInterruptSource::write(uint16_t address, uint8_t value) {
	address &= 15;
	switch(address) {
		default: break;

		case 0: case 2:
			channels_[address >> 1].reload = (channels_[address >> 1].reload & 0xff00) | value;
		break;
		case 1:	case 3:
			channels_[address >> 1].reload = uint16_t((channels_[address >> 1].reload & 0x00ff) | ((value & 0xf) << 8));
		break;

		case 7:
			channels_[0].sync = value & 0x01;
			channels_[1].sync = value & 0x02;
			rate_ = InterruptRate((value >> 5) & 3);
		break;
	}
}

void TimedInterruptSource::run_for(Cycles cycles) {
	// Update the 1Hz interrupt.
	one_hz_offset_ -= cycles;
	if(one_hz_offset_ <= Cycles(0)) {
		interrupts_ |= uint8_t(Interrupt::OneHz);
		one_hz_offset_ += clock_rate;
	}

	// TODO: update the programmable-frequency interrupt.
}

Cycles TimedInterruptSource::get_next_sequence_point() const {
	// TODO: support the programmable-frequency interrupt.
	return one_hz_offset_;
}
