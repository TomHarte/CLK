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
	address &= 0x1f;
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

			case 31:
				global_divider_reload_ = 2 + ((value >> 1)&1);
			break;
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
	int16_t output_level[2];
	size_t c = 0;
	while(c < number_of_samples) {
		// I'm unclear on the details of the time division multiplexing so,
		// for now, just sum the outputs.
		output_level[0] =
			volume_ *
				(use_direct_output_[0] ?
					channels_[0].amplitude[0]
					: (
						channels_[0].amplitude[0] * (channels_[0].output & 1) +
						channels_[1].amplitude[0] * (channels_[1].output & 1) +
						channels_[2].amplitude[0] * (channels_[2].output & 1) +
						noise_.amplitude[0] * noise_.final_output
				));

		output_level[1] =
			volume_ *
				(use_direct_output_[1] ?
					channels_[0].amplitude[1]
					: (
						channels_[0].amplitude[1] * (channels_[0].output & 1) +
						channels_[1].amplitude[1] * (channels_[1].output & 1) +
						channels_[2].amplitude[1] * (channels_[2].output & 1) +
						noise_.amplitude[1] * noise_.final_output
				));

		while(global_divider_ && c < number_of_samples) {
			--global_divider_;
			*reinterpret_cast<uint32_t *>(&target[c << 1]) = *reinterpret_cast<uint32_t *>(output_level);
			++c;
		}

		global_divider_ = global_divider_reload_;
		if(!global_divider_) {
			global_divider_ = global_divider_reload_;
		}
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
		int noise_output = noise_.output & 1;
		noise_.output <<= 1;
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

			noise_output = poly_state_[int(Channel::Distortion::None)];
		}
		noise_.output |= noise_output;

		// Low pass: sample channel 2 on downward transitions of the prima facie output.
		if(noise_.low_pass && (noise_.output & 3) == 2) {
			noise_.output = (noise_.output & ~1) | (channels_[2].output & 1);
		}

		// Apply noise high-pass.
		if(noise_.high_pass && (channels_[0].output & 3) == 2) {
			noise_.output &= ~1;
		}

		// Update noise ring modulation, if any.
		if(noise_.ring_modulate) {
			noise_.final_output = !((noise_.output ^ channels_[1].output) & 1);
		} else {
			noise_.final_output = noise_.output & 1;
		}
	}
}

// MARK: - Interrupt source

uint8_t TimedInterruptSource::get_new_interrupts() {
	const uint8_t result = interrupts_;
	interrupts_ = 0;
	return result;
}

void TimedInterruptSource::write(uint16_t address, uint8_t value) {
	address &= 0x1f;
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

		case 31:
			global_divider_ = Cycles(2 + ((value >> 1)&1));
		break;
	}
}

void TimedInterruptSource::update_channel(int c, bool is_linked, int decrement) {
	if(channels_[c].sync) {
		channels_[c].value = channels_[c].reload;
	} else {
		if(decrement <= channels_[c].value) {
			channels_[c].value -= decrement;
		} else {
			// The decrement is greater than the current value, therefore
			// there'll be at least one flip.
			//
			// After decreasing the decrement by the current value + 1,
			// it'll be clear how many decrements are left after reload.
			//
			// Dividing that by the number of decrements necessary for a
			// flip will provide the total number of flips.
			const int decrements_after_flip = decrement - (channels_[c].value + 1);
			const int num_flips = 1 + decrements_after_flip / (channels_[c].reload + 1);

			// If this is a linked channel, set the interrupt mask if a transition
			// from high to low is amongst the included flips.
			if(is_linked && num_flips + channels_[c].level >= 2) {
				interrupts_ |= uint8_t(Interrupt::VariableFrequency);
				programmable_level_ ^= true;
			}
			channels_[c].level ^= (num_flips & 1);

			// Apply the modulo number of decrements to the reload value to
			// figure out where things stand now.
			channels_[c].value = channels_[c].reload - decrements_after_flip % (channels_[c].reload + 1);
		}
	}
}

void TimedInterruptSource::run_for(Cycles duration) {
	// Determine total number of ticks.
	run_length_ += duration;
	const Cycles cycles = run_length_.divide(global_divider_);
	if(cycles == Cycles(0)) {
		return;
	}

	// Update the two-second counter, from which the 1Hz, 50Hz and 1000Hz signals
	// are derived.
	const int previous_counter = two_second_counter_;
	two_second_counter_ = (two_second_counter_ + cycles.as<int>()) % 500'000;

	// Check for a 1Hz rollover.
	if(previous_counter / 250'000 != two_second_counter_ / 250'000) {
		interrupts_ |= uint8_t(Interrupt::OneHz);
	}

	// Check for 1kHz or 50Hz rollover;
	switch(rate_) {
		default: break;
		case InterruptRate::OnekHz:
			if(previous_counter / 250 != two_second_counter_ / 250) {
				interrupts_ |= uint8_t(Interrupt::VariableFrequency);
				programmable_level_ ^= true;
			}
		break;
		case InterruptRate::FiftyHz:
			if(previous_counter / 5'000 != two_second_counter_ / 5'000) {
				interrupts_ |= uint8_t(Interrupt::VariableFrequency);
				programmable_level_ ^= true;
			}
		break;
	}

	// Update the two tone channels.
	update_channel(0, rate_ == InterruptRate::ToneGenerator0, cycles.as<int>());
	update_channel(1, rate_ == InterruptRate::ToneGenerator1, cycles.as<int>());
}

Cycles TimedInterruptSource::get_next_sequence_point() const {
	// Since both the 1kHz and 50Hz timers are integer dividers of the 1Hz timer, there's no need
	// to factor that one in when determining the next sequence point for either of those.
	switch(rate_) {
		default:
		case InterruptRate::OnekHz:		return Cycles(250 - (two_second_counter_ % 250));
		case InterruptRate::FiftyHz:	return Cycles(5000 - (two_second_counter_ % 5000));

		case InterruptRate::ToneGenerator0:
		case InterruptRate::ToneGenerator1: {
			const auto &channel = channels_[int(rate_) - int(InterruptRate::ToneGenerator0)];
			const int cycles_until_interrupt = channel.value + 1 + (!channel.level) * (channel.reload + 1);

			return Cycles(std::min(
				250'000 - (two_second_counter_ % 250'000),
				cycles_until_interrupt
			));
		}
	}
}

uint8_t TimedInterruptSource::get_divider_state() {
	return uint8_t((two_second_counter_ / 250'000) * 4 | programmable_level_);
}
