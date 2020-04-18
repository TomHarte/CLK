//
//  Channel.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 15/04/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#include "Channel.hpp"
#include "Tables.h"

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

int Channel::update(Operator *modulator, Operator *carrier, OperatorOverrides *modulator_overrides, OperatorOverrides *carrier_overrides) {
	modulator->update(modulator_state_, key_on_, period_ << frequency_shift_, octave_, modulator_overrides);
	carrier->update(carrier_state_, key_on_, period_ << frequency_shift_, octave_, carrier_overrides);

	const auto modulator_level = level(modulator_state_);
	if(use_fm_synthesis_) {
		return level(carrier_state_, modulator_level) << 2;
	} else {
		return (modulator_level + level(carrier_state_)) << 1;
	}
}

bool Channel::is_audible(Operator *carrier, OperatorOverrides *carrier_overrides) {
	return carrier->is_audible(carrier_state_, carrier_overrides);
}

int Channel::level(OperatorState &state, int modulator_level) {
	const auto log_sin  = negative_log_sin(modulator_level + state.phase);
	return power_two(log_sin.logsin + state.attenuation) * log_sin.sign;
}
