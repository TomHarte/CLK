//
//  Channel.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 15/04/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#include "Channel.hpp"

using namespace Yamaha::OPL;

void Channel::set_frequency_low(uint8_t value) {
	period_ = (period_ &~0xff) | value;
}

void Channel::set_10bit_frequency_octave_key_on(uint8_t value) {
	period_ = (period_ & 0xff) | ((value & 3) << 8);
	octave_ = (value >> 2) & 0x7;
	key_on_ = value & 0x20;
	frequency_shift_ = 0;
}

void Channel::set_9bit_frequency_octave_key_on(uint8_t value) {
	period_ = (period_ & 0xff) | ((value & 1) << 8);
	octave_ = (value >> 1) & 0x7;
	key_on_ = value & 0x10;;
	frequency_shift_ = 1;
}

void Channel::set_feedback_mode(uint8_t value) {
	feedback_strength_ = (value >> 1) & 0x7;
	use_fm_synthesis_ = value & 1;
}

void Channel::update(bool modulator, const LowFrequencyOscillator &oscillator, Operator &op, bool force_key_on, OperatorOverrides *overrides) {
	op.update(states_[int(modulator)], oscillator, key_on_ || force_key_on, period_ << frequency_shift_, octave_, overrides);
}

int Channel::melodic_output(Operator &modulator, Operator &carrier, OperatorOverrides *modulator_overrides, OperatorOverrides *carrier_overrides) {
	if(use_fm_synthesis_) {
		// Get modulator level, use that as a phase-adjusting input to the carrier and then return the carrier level.
		const LogSign modulator_output = modulator.melodic_output(states_[1]);
		return carrier.melodic_output(states_[0], &modulator_output).level();
	} else {
		// Get modulator and carrier levels separately, return their sum.
		return (carrier.melodic_output(states_[0]).level() + modulator.melodic_output(states_[1]).level()) >> 1;
	}
}

int Channel::tom_tom_output(Operator &modulator, OperatorOverrides *modulator_overrides) {
	return modulator.melodic_output(states_[1]).level();
}

int Channel::snare_output(Operator &carrier, OperatorOverrides *carrier_overrides) {
	return carrier.snare_output(states_[0]).level();
}

int Channel::cymbal_output(Operator &modulator, Operator &carrier, Channel &channel8, OperatorOverrides *modulator_overrides) {
	return carrier.cymbal_output(states_[0], channel8.states_[1]).level();
}

int Channel::high_hat_output(Operator &modulator, Operator &carrier, Channel &channel8, OperatorOverrides *modulator_overrides) {
	return carrier.high_hat_output(states_[0], channel8.states_[1]).level();
}

bool Channel::is_audible(Operator *carrier, OperatorOverrides *carrier_overrides) {
	return carrier->is_audible(states_[0], carrier_overrides);
}
