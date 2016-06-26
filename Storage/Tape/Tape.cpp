//
//  Tape.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Tape.hpp"

using namespace Storage;

void Tape::seek(Tape::Time seek_time)
{
	// TODO: as best we can
}

TapePlayer::TapePlayer(unsigned int input_clock_rate) :
	_input_clock_rate(input_clock_rate)
{}

void TapePlayer::set_tape(std::shared_ptr<Storage::Tape> tape)
{
	_tape = tape;
	get_next_pulse();
}

bool TapePlayer::has_tape()
{
	return (bool)_tape;
}

void TapePlayer::get_next_pulse()
{
	_input.time_into_pulse = 0;
	if(_tape)
		_input.current_pulse = _tape->get_next_pulse();
	else
	{
		_input.current_pulse.length.length = 1;
		_input.current_pulse.length.clock_rate = 1;
		_input.current_pulse.type = Storage::Tape::Pulse::Zero;
	}
	if(_input.pulse_stepper == nullptr || _input.current_pulse.length.clock_rate != _input.pulse_stepper->get_output_rate())
	{
		_input.pulse_stepper.reset(new SignalProcessing::Stepper(_input.current_pulse.length.clock_rate, _input_clock_rate));
	}
}

void TapePlayer::run_for_cycles(unsigned int number_of_cycles)
{
	if(has_tape())
	{
		_input.time_into_pulse += (unsigned int)_input.pulse_stepper->step();
		if(_input.time_into_pulse == _input.current_pulse.length.length)
		{
			run_for_input_pulse();
		}
	}
}

void TapePlayer::run_for_input_pulse()
{
	process_input_pulse(_input.current_pulse);
	get_next_pulse();
}
