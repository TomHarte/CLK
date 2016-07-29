//
//  Tape.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Tape.hpp"
#include "../../NumberTheory/Factors.hpp"

using namespace Storage;

void Tape::seek(Time seek_time)
{
	// TODO: as best we can
}

TapePlayer::TapePlayer(unsigned int input_clock_rate) :
	_input_clock_rate(input_clock_rate)
{}

void TapePlayer::set_tape(std::shared_ptr<Storage::Tape> tape)
{
	_tape = tape;
	_input.pulse_stepper.reset();

	get_next_pulse();
}

bool TapePlayer::has_tape()
{
	return (bool)_tape;
}

void TapePlayer::get_next_pulse()
{
	unsigned int previous_clock_rate = 0;

	// figure out how much time has been run since the last bit ended
	if(_input.pulse_stepper == nullptr)
		_input.time_into_pulse = 0;
	else
	{
		_input.time_into_pulse -= _input.current_pulse.length.length;
		previous_clock_rate = _input.current_pulse.length.clock_rate;
	}

	// get the new pulse
	if(_tape)
		_input.current_pulse = _tape->get_next_pulse();
	else
	{
		_input.current_pulse.length.length = 1;
		_input.current_pulse.length.clock_rate = 1;
		_input.current_pulse.type = Storage::Tape::Pulse::Zero;
	}

	// if there was any time left over, map into the new time base
	if(_input.pulse_stepper && _input.time_into_pulse)
	{
		// simplify the quotient
		unsigned int common_divisor = NumberTheory::greatest_common_divisor(_input.time_into_pulse, previous_clock_rate);
		_input.time_into_pulse /= common_divisor;
		previous_clock_rate /= common_divisor;

		// build a quotient that is the sum of the time overrun plus the incoming time and adjust the time overrun
		// to be in terms of the new quotient
		unsigned int denominator = NumberTheory::least_common_multiple(previous_clock_rate, _input.current_pulse.length.clock_rate);
		_input.current_pulse.length.length *= denominator / _input.current_pulse.length.clock_rate;
		_input.current_pulse.length.clock_rate = denominator;
		_input.time_into_pulse *= denominator / previous_clock_rate;
	}

	// adjust stepper if required
	if(_input.pulse_stepper == nullptr || _input.current_pulse.length.clock_rate != _input.pulse_stepper->get_output_rate())
	{
		_input.pulse_stepper.reset(new SignalProcessing::Stepper(_input.current_pulse.length.clock_rate, _input_clock_rate));
	}
}

void TapePlayer::run_for_cycles(unsigned int number_of_cycles)
{
	if(has_tape())
	{
		_input.time_into_pulse += (unsigned int)_input.pulse_stepper->step(number_of_cycles);
		while(_input.time_into_pulse >= _input.current_pulse.length.length)
		{
			run_for_input_pulse();
		}
	}
}

void TapePlayer::run_for_input_pulse()
{
	process_input_pulse(_input.current_pulse);
	get_next_pulse();
	_input.time_into_pulse = 0;
}
