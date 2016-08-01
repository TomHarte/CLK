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
	TimedEventLoop(input_clock_rate)
{}

void TapePlayer::set_tape(std::shared_ptr<Storage::Tape> tape)
{
	_tape = tape;
	reset_timer();
	get_next_pulse();
}

bool TapePlayer::has_tape()
{
	return (bool)_tape;
}

void TapePlayer::get_next_pulse()
{
	// get the new pulse
	if(_tape)
		_current_pulse = _tape->get_next_pulse();
	else
	{
		_current_pulse.length.length = 1;
		_current_pulse.length.clock_rate = 1;
		_current_pulse.type = Storage::Tape::Pulse::Zero;
	}

	set_next_event_time_interval(_current_pulse.length);
}

void TapePlayer::run_for_cycles(int number_of_cycles)
{
	if(has_tape())
	{
		::TimedEventLoop::run_for_cycles(number_of_cycles);
	}
}

void TapePlayer::run_for_input_pulse()
{
	jump_to_next_event();
}

void TapePlayer::process_next_event()
{
	process_input_pulse(_current_pulse);
	get_next_pulse();
}
