//
//  OPL2.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 02/04/2020.
//  Copyright © 2020 Thomas Harte. all rights reserved.
//

#include "OPL2.hpp"

#include <cassert>
#include <cmath>

namespace {

/*

	Credit for the fixed register lists goes to Nuke.YKT; I found them at:
	https://siliconpr0n.org/archive/doku.php?id=vendor:yamaha:opl2#ym2413_instrument_rom

	The arrays below begin with channel 1, each line is a single channel and the
	format per channel is, from first byte to eighth:

		Bytes 1 and 2:
			Registers 1 and 2, i.e. modulator and carrier
			amplitude modulation select, vibrato select, etc.

		Byte 3:
			b7, b6: modulator key scale level
			b5...b0: modulator total level (inverted)

		Byte 4:
			b7: carrier key scale level
			b3...b0: feedback level and waveform selects as per register 4

		Bytes 5, 6:
			Registers 4 and 5, i.e. decay and attack rate, modulator and carrier.

		Bytes 7, 8:
			Registers 6 and 7, i.e. decay-sustain level and release rate, modulator and carrier.

*/

constexpr uint8_t opll_patch_set[] = {
	0x71, 0x61, 0x1e, 0x17, 0xd0, 0x78, 0x00, 0x17,
	0x13, 0x41, 0x1a, 0x0d, 0xd8, 0xf7, 0x23, 0x13,
	0x13, 0x01, 0x99, 0x00, 0xf2, 0xc4, 0x11, 0x23,
	0x31, 0x61, 0x0e, 0x07, 0xa8, 0x64, 0x70, 0x27,
	0x32, 0x21, 0x1e, 0x06, 0xe0, 0x76, 0x00, 0x28,
	0x31, 0x22, 0x16, 0x05, 0xe0, 0x71, 0x00, 0x18,
	0x21, 0x61, 0x1d, 0x07, 0x82, 0x81, 0x10, 0x07,
	0x23, 0x21, 0x2d, 0x14, 0xa2, 0x72, 0x00, 0x07,
	0x61, 0x61, 0x1b, 0x06, 0x64, 0x65, 0x10, 0x17,
	0x41, 0x61, 0x0b, 0x18, 0x85, 0xf7, 0x71, 0x07,
	0x13, 0x01, 0x83, 0x11, 0xfa, 0xe4, 0x10, 0x04,
	0x17, 0xc1, 0x24, 0x07, 0xf8, 0xf8, 0x22, 0x12,
	0x61, 0x50, 0x0c, 0x05, 0xc2, 0xf5, 0x20, 0x42,
	0x01, 0x01, 0x55, 0x03, 0xc9, 0x95, 0x03, 0x02,
	0x61, 0x41, 0x89, 0x03, 0xf1, 0xe4, 0x40, 0x13,
};

constexpr uint8_t vrc7_patch_set[] = {
	0x03, 0x21, 0x05, 0x06, 0xe8, 0x81, 0x42, 0x27,
	0x13, 0x41, 0x14, 0x0d, 0xd8, 0xf6, 0x23, 0x12,
	0x11, 0x11, 0x08, 0x08, 0xfa, 0xb2, 0x20, 0x12,
	0x31, 0x61, 0x0c, 0x07, 0xa8, 0x64, 0x61, 0x27,
	0x32, 0x21, 0x1e, 0x06, 0xe1, 0x76, 0x01, 0x28,
	0x02, 0x01, 0x06, 0x00, 0xa3, 0xe2, 0xf4, 0xf4,
	0x21, 0x61, 0x1d, 0x07, 0x82, 0x81, 0x11, 0x07,
	0x23, 0x21, 0x22, 0x17, 0xa2, 0x72, 0x01, 0x17,
	0x35, 0x11, 0x25, 0x00, 0x40, 0x73, 0x72, 0x01,
	0xb5, 0x01, 0x0f, 0x0f, 0xa8, 0xa5, 0x51, 0x02,
	0x17, 0xc1, 0x24, 0x07, 0xf8, 0xf8, 0x22, 0x12,
	0x71, 0x23, 0x11, 0x06, 0x65, 0x74, 0x18, 0x16,
	0x01, 0x02, 0xd3, 0x05, 0xc9, 0x95, 0x03, 0x02,
	0x61, 0x63, 0x0c, 0x00, 0x94, 0xc0, 0x33, 0xf6,
	0x21, 0x72, 0x0d, 0x00, 0xc1, 0xd5, 0x56, 0x06,
};

constexpr uint8_t percussion_patch_set[] = {
	0x01, 0x01, 0x18, 0x0f, 0xdf, 0xf8, 0x6a, 0x6d,
	0x01, 0x01, 0x00, 0x00, 0xc8, 0xd8, 0xa7, 0x48,
	0x05, 0x01, 0x00, 0x00, 0xf8, 0xaa, 0x59, 0x55,
};

}

using namespace Yamaha::OPL;


template <typename Child>
OPLBase<Child>::OPLBase(Concurrency::DeferringAsyncTaskQueue &task_queue) : task_queue_(task_queue) {
	// Populate the exponential and log-sine tables; formulas here taken from Matthew Gambrell
	// and Olli Niemitalo's decapping and reverse-engineering of the OPL2.
	for(int c = 0; c < 256; ++c) {
		exponential_[c] = int(round((pow(2.0, double(c) / 256.0) - 1.0) * 1024.0));

		const double sine = sin((double(c) + 0.5) * M_PI/512.0);
		log_sin_[c] = int(
			round(
				-log(sine) / log(2.0) * 256.0
			 )
		);
	}
}

template <typename Child>
void OPLBase<Child>::write(uint16_t address, uint8_t value) {
	if(address & 1) {
		static_cast<Child *>(this)->write_register(selected_register_, value);
	} else {
		selected_register_ = value;
	}
}

template class Yamaha::OPL::OPLBase<Yamaha::OPL::OPLL>;
template class Yamaha::OPL::OPLBase<Yamaha::OPL::OPL2>;


OPLL::OPLL(Concurrency::DeferringAsyncTaskQueue &task_queue, int audio_divider, bool is_vrc7): OPLBase(task_queue), audio_divider_(audio_divider) {
	// Due to the way that sound mixing works on the OPLL, the audio divider may not
	// be larger than 2.
	assert(audio_divider <= 2);

	// Install fixed instruments.
	const uint8_t *patch_set = is_vrc7 ? vrc7_patch_set : opll_patch_set;
	for(int c = 0; c < 15; ++c) {
		setup_fixed_instrument(c+1, patch_set);
		patch_set += 8;
	}

	// Install rhythm patches.
	for(int c = 0; c < 3; ++c) {
		setup_fixed_instrument(c+16, &percussion_patch_set[c * 8]);
	}

	// Set default modulators.
	for(int c = 0; c < 9; ++c) {
		channels_[c].modulator = &operators_[0];
	}
}

bool OPLL::is_zero_level() {
//	for(int c = 0; c < 9; ++c) {
//		if(channels_[c].is_audible()) return false;
//	}
	return false;
}

void OPLL::get_samples(std::size_t number_of_samples, std::int16_t *target) {
	// Both the OPLL and the OPL2 divide the input clock by 72 to get the base tick frequency;
	// unlike the OPL2 the OPLL time-divides the output for 'mixing'.

	const int update_period = 72 / audio_divider_;
	const int channel_output_period = 8 / audio_divider_;

	// Fill in any leftover from the previous session.
	if(audio_offset_) {
		while(audio_offset_ < update_period && number_of_samples) {
			*target = int16_t(channels_[audio_offset_ / channel_output_period].level);
			++target;
			++audio_offset_;
			--number_of_samples;
		}
		audio_offset_ = 0;
	}

	// End now if that provided everything that was asked for.
	if(!number_of_samples) return;

	int total_updates = int(number_of_samples) / update_period;
	number_of_samples %= size_t(update_period);
	audio_offset_ = int(number_of_samples);

	while(total_updates--) {
		update_all_chanels();

		for(int c = 0; c < update_period; ++c) {
			*target = int16_t(channels_[c / channel_output_period].level);
			++target;
		}
	}

	// If there are any other spots remaining, fill them.
	if(number_of_samples) {
		update_all_chanels();

		for(int c = 0; c < int(number_of_samples); ++c) {
			*target = int16_t(channels_[c / channel_output_period].level);
			++target;
		}
	}
}

void OPLL::set_sample_volume_range(std::int16_t range) {
	total_volume_ = range;
}

uint8_t OPLL::read(uint16_t address) {
	// I've seen mention of an undocumented two-bit status register. I don't yet know what is in it.
	return 0xff;
}

void OPLL::write_register(uint8_t address, uint8_t value) {
	// The OPLL doesn't have timers or other non-audio functions, so all writes
	// go to the audio queue.
	task_queue_.defer([this, address, value] {
		// The first 8 locations are used to define the custom instrument, and have
		// exactly the same format as the patch set arrays at the head of this file.
		if(address < 8) {
			custom_instrument_[address] = value;

			// Update whatever that did to the instrument.
			setup_fixed_instrument(0, custom_instrument_);
			return;
		}

		// Register 0xe is a cut-down version of the OPLL's register 0xbd.
		if(address == 0xe) {
			depth_rhythm_control_ = value & 0x3f;
			return;
		}

		const auto index = address & 0xf;
		if(index > 8) return;

		switch(address & 0xf0) {
			case 0x30:
				// Select an instrument in the top nibble, set a channel volume in the lower.
				channels_[index].overrides.attenuation = value & 0xf;
				channels_[index].modulator = &operators_[(value >> 4) * 2];
			break;

			case 0x10:	channels_[index].set_frequency_low(value);	break;

			case 0x20:
				// Set sustain on/off, key on/off, octave and a single extra bit of frequency.
				// So they're a lot like OPLL registers 0xb0 to 0xb8, but not identical.
				channels_[index].set_9bit_frequency_octave_key_on(value);
				channels_[index].overrides.hold_sustain_level = value & 0x20;
			break;

			default: break;
		}
	});
}

void OPLL::setup_fixed_instrument(int number, const uint8_t *data) {
	auto modulator = &operators_[number * 2];
	auto carrier = &operators_[number * 2 + 1];

	modulator->set_am_vibrato_hold_sustain_ksr_multiple(data[0]);
	carrier->set_am_vibrato_hold_sustain_ksr_multiple(data[1]);
	modulator->set_scaling_output(data[2]);

	// Set waveforms — only sine and halfsine are available.
	carrier->set_waveform((data[3] >> 4) & 1);
	modulator->set_waveform((data[3] >> 3) & 1);

	// TODO: data[3] b0-b2: modulator feedback level
	// TODO: data[3] b6, b7: carrier key-scale level

	// Set ADSR parameters.
	modulator->set_attack_decay(data[4]);
	carrier->set_attack_decay(data[5]);
	modulator->set_sustain_release(data[6]);
	carrier->set_sustain_release(data[7]);
}

/*
template <Personality personality>
void OPL2<personality>::get_samples(std::size_t number_of_samples, std::int16_t *target) {
	// TODO.
	//  out = exp(logsin(phase2 + exp(logsin(phase1) + gain1)) + gain2)

//		Melodic channels are:
//
//		Channel		Operator 1		Operator 2
//		0			0				3
//		1			1				4
//		2			2				5
//		3			6				9
//		4			7				10
//		5			8				11
//		6			12				15
//		7			13				16
//		8			14				17
//
//		In percussion mode, only channels 0–5 are use as melodic, with 6, 7 and 8 being
//		replaced by:
//
//		Bass drum, using operators 12 and 15;
//		Snare, using operator 16;
//		Tom tom, using operator 14,
//		Cymbal, using operator 17; and
//		Symbol, using operator 13.
}

*/

void OPL2::write_register(uint8_t address, uint8_t value) {

	// Deal with timer changes synchronously.
	switch(address) {
		case 0x02:	timers_[0] = value; 	return;
		case 0x03:	timers_[1] = value;		return;
		case 0x04:	timer_control_ = value;	return;
		// TODO from register 4:
		//	b7 = IRQ reset;
		//	b6/b5 = timer 1/2 mask (irq enabling flags, I think?)
		//	b4/b3 = timer 2/1 start (seemingly the opposite order to b6/b5?)

		default: break;
	}

	// Enqueue any changes that affect audio output.
	task_queue_.enqueue([this, address, value] {
		//
		// Modal modifications.
		//

		switch(address) {
			case 0x01:	waveform_enable_ = value & 0x20;	break;
			case 0x08:
				// b7: "composite sine wave mode on/off"?
				csm_keyboard_split_ = value;
				// b6: "Controls the split point of the keyboard. When 0, the keyboard split is the
				// second bit from the bit 8 of the F-Number. When 1, the MSB of the F-Number is used."
			break;
			case 0xbd:	depth_rhythm_control_ = value;		break;

			default: break;
		}


		//
		// Operator modifications.
		//

		if((address >= 0x20 && address < 0xa0) || address >= 0xe0) {
			// The 18 operators are spreat out across 22 addresses; each group of
			// six is framed within an eight-byte area thusly:
			constexpr int operator_by_address[] = {
				0,	1,	2,	3,	4,	5,	-1,	-1,
				6,	7,	8,	9,	10,	11,	-1,	-1,
				12,	13,	14,	15,	16,	17,	-1,	-1,
				-1,	-1, -1, -1, -1, -1, -1, -1,
			};

			const auto index = operator_by_address[address & 0x1f];
			if(index == -1) return;

			switch(address & 0xe0) {
				case 0x20:	operators_[index].set_am_vibrato_hold_sustain_ksr_multiple(value);	break;
				case 0x40:	operators_[index].set_scaling_output(value);						break;
				case 0x60:	operators_[index].set_attack_decay(value);							break;
				case 0x80:	operators_[index].set_sustain_release(value);						break;
				case 0xe0:	operators_[index].set_waveform(value);								break;

				default: break;
			}
		}


		//
		// Channel modifications.
		//

		const auto index = address & 0xf;
		if(index > 8) return;

		switch(address & 0xf0) {
			case 0xa0:	channels_[index].set_frequency_low(value);					break;
			case 0xb0:	channels_[index].set_10bit_frequency_octave_key_on(value);	break;
			case 0xc0:	channels_[index].set_feedback_mode(value);					break;

			default: break;
		}
	});
}

uint8_t OPL2::read(uint16_t address) {
	// TODO. There's a status register where:
	//	b7 = IRQ status (set if interrupt request ongoing)
	//	b6 = timer 1 flag (set if timer 1 expired)
	//	b5 = timer 2 flag
	return 0xff;
}

// MARK: - Operators

void Operator::update(OperatorState &state, bool key_on, int channel_period, int channel_octave, OperatorOverrides *overrides) {
	// Per the documentation:
	//
	// Delta phase = ( [desired freq] * 2^19 / [input clock / 72] ) / 2 ^ (b - 1)
	//
	// After experimentation, I think this gives rate calculation as formulated below.

	// This encodes the MUL -> multiple table given on page 12,
	// multiplied by two.
	constexpr int multipliers[] = {
		1, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 20, 24, 24, 30, 30
	};

	// Update the raw phase.
	// TODO: if this is the real formula (i.e. a downward shift for channel_octave), this is a highly
	// suboptimal way to do this. Could just keep one accumulator and shift that downward for the result.
	const int octave_divider = 2048 >> channel_octave;
	state.divider_ %= octave_divider;
	state.divider_ += channel_period;
	state.raw_phase_ += multipliers[frequency_multiple] * (state.divider_ / octave_divider);
	// TODO: this last step introduces aliasing, but is a quick way to verify whether the multiplier should
	// be applied also to the octave.

	// Hence calculate phase (TODO: by also taking account of vibrato).
	constexpr int waveforms[4][4] = {
		{1023, 1023, 1023, 1023},	// Sine: don't mask in any quadrant.
		{511, 511, 0, 0},			// Half sine: keep the first half in tact, lock to 0 in the second half.
		{511, 511, 511, 511},		// AbsSine: endlessly repeat the first half of the sine wave.
		{255, 0, 255, 0},			// PulseSine: act as if the first quadrant is in the first and third; lock the other two to 0.
	};
	state.phase = state.raw_phase_ & waveforms[int(waveform)][(state.raw_phase_ >> 8) & 3];

	// Key-on logic: any time it is false, be in the release state.
	// On the leading edge of it becoming true, enter the attack state.
	if(!key_on) {
		state.adsr_phase_ = OperatorState::ADSRPhase::Release;
		state.time_in_phase_ = 0;
	} else if(!state.last_key_on_) {
		state.adsr_phase_ = OperatorState::ADSRPhase::Attack;
		state.time_in_phase_ = 0;
	}
	state.last_key_on_ = key_on;

	// Adjust the ADSR attenuation appropriately;
	// cf. http://forums.submarine.org.uk/phpBB/viewtopic.php?f=9&t=16 (primarily) for the source of the maths below.

	// "An attack rate value of 52 (AR = 13) has 32 samples in the attack phase, an attack rate value of 48 (AR = 12)
	// has 64 samples in the attack phase, but pairs of samples show the same envelope attenuation. I am however struggling to find a plausible algorithm to match the experimental results.

	const auto current_phase = state.adsr_phase_;
	switch(current_phase) {
		case OperatorState::ADSRPhase::Attack: {
			const int attack_rate = attack_rate_;	// TODO: key scaling rate. Which I do not yet understand.

			// Rules:
			//
			// An attack rate of '13' has 32 samples in the attack phase; a rate of '12' has the same 32 steps, but spread out over 64 samples, etc.
			// An attack rate of '14' uses a divide by four instead of two.
			// 15 is instantaneous.

			if(attack_rate >= 56) {
				state.adsr_attenuation_ = state.adsr_attenuation_ - (state.adsr_attenuation_ >> 2) - 1;
			} else {
				const int sample_length = 1 << (14 - (attack_rate >> 2));	// TODO: don't throw away KSR bits.
				if(!(state.time_in_phase_ & (sample_length - 1))) {
					state.adsr_attenuation_ = state.adsr_attenuation_ - (state.adsr_attenuation_ >> 3) - 1;
				}
			}

			// Two possible terminating conditions: (i) the attack rate is 15; (ii) full volume has been reached.
			if(attack_rate > 60 || state.adsr_attenuation_ <= 0) {
				state.adsr_attenuation_ = 0;
				state.adsr_phase_ = OperatorState::ADSRPhase::Decay;
			}
		} break;

		case OperatorState::ADSRPhase::Release:
		case OperatorState::ADSRPhase::Decay:
		{
			// Rules:
			//
			// (relative to a 511 scale)
			//
			// A rate of 0 is no decay at all.
			// A rate of 1 means increase 4 per cycle.
			// A rate of 2 means increase 2 per cycle.
			// A rate of 3 means increase 1 per cycle.
			// A rate of 4 means increase 1 every other cycle.
			// (etc)
			const int decrease_rate = (state.adsr_phase_ == OperatorState::ADSRPhase::Decay) ? decay_rate_ : release_rate_;	// TODO: again, key scaling rate.

			if(decrease_rate) {
				// TODO: don't throw away KSR bits.
				switch(decrease_rate >> 2) {
					case 1: state.adsr_attenuation_ += 4;	break;
					case 2: state.adsr_attenuation_ += 2;	break;
					default: {
						const int sample_length = 1 << ((decrease_rate >> 2) - 4);
						if(!(state.time_in_phase_ & (sample_length - 1))) {
							++state.adsr_attenuation_;
						}
					} break;
				}
			}

			// Clamp to the proper range.
			state.adsr_attenuation_ = std::min(state.adsr_attenuation_, 511);

			// Check for the decay exit condition.
			if(state.adsr_phase_ == OperatorState::ADSRPhase::Decay && state.adsr_attenuation_ >= (sustain_level_ << 5)) {
				state.adsr_attenuation_ = sustain_level_ << 5;
				state.adsr_phase_ = ((overrides && overrides->hold_sustain_level) || hold_sustain_level) ? OperatorState::ADSRPhase::Sustain : OperatorState::ADSRPhase::Release;
			}
		} break;

		case OperatorState::ADSRPhase::Sustain:
			// Nothing to do.
		break;
	}
	if(state.adsr_phase_ == current_phase) {
		++state.time_in_phase_;
	} else {
		state.time_in_phase_ = 0;
	}

	// Combine the ADSR attenuation and overall channel attenuation, clamping to the permitted range.
	if(overrides) {
		state.attenuation = state.adsr_attenuation_ + (overrides->attenuation << 4);
	} else {
		state.attenuation = state.adsr_attenuation_ + (attenuation_ << 2);
	}
}

