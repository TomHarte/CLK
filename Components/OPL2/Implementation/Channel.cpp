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

int Channel::update_melodic(const LowFrequencyOscillator &oscillator, Operator *modulator, Operator *carrier, bool force_key_on, OperatorOverrides *modulator_overrides, OperatorOverrides *carrier_overrides) {
	modulator->update(modulator_state_, oscillator, key_on_ || force_key_on, period_ << frequency_shift_, octave_, modulator_overrides);
	carrier->update(carrier_state_, oscillator, key_on_ || force_key_on, period_ << frequency_shift_, octave_, carrier_overrides);

	if(use_fm_synthesis_) {
		// Get modulator level, use that as a phase-adjusting input to the carrier and then return the carrier level.
		const LogSign modulator_output = modulator->melodic_output(modulator_state_);
		return carrier->melodic_output(carrier_state_, &modulator_output).level();
	} else {
		// Get modulator and carrier levels separately, return their sum.
		return (carrier->melodic_output(carrier_state_).level() + modulator->melodic_output(carrier_state_).level()) >> 1;
	}
}

int Channel::update_tom_tom(const LowFrequencyOscillator &oscillator, Operator *modulator, bool force_key_on, OperatorOverrides *modulator_overrides) {
	modulator->update(modulator_state_, oscillator, key_on_ || force_key_on, period_ << frequency_shift_, octave_, modulator_overrides);
	return modulator->melodic_output(modulator_state_).level();
}

int Channel::update_snare(const LowFrequencyOscillator &oscillator, Operator *carrier, bool force_key_on, OperatorOverrides *carrier_overrides) {
	carrier->update(carrier_state_, oscillator, key_on_ || force_key_on, period_ << frequency_shift_, octave_, carrier_overrides);
	return carrier->snare_output(modulator_state_).level();
}

bool Channel::is_audible(Operator *carrier, OperatorOverrides *carrier_overrides) {
	return carrier->is_audible(carrier_state_, carrier_overrides);
}
