//
//  DiskController.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/07/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "DiskController.hpp"

using namespace Storage::Disk;

Controller::Controller(unsigned int clock_rate, unsigned int clock_rate_multiplier, unsigned int revolutions_per_minute) :
	_clock_rate(clock_rate * clock_rate_multiplier),
	_clock_rate_multiplier(clock_rate_multiplier),

	TimedEventLoop(clock_rate * clock_rate_multiplier)
{
	_rotational_multiplier.length = 60;
	_rotational_multiplier.clock_rate = revolutions_per_minute;
	_rotational_multiplier.simplify();

	// seed this class with a PLL, any PLL, so that it's safe to assume non-nullptr later
	Time one;
	set_expected_bit_length(one);
}

void Controller::setup_track()	// Time initial_offset
{
	_track = _drive->get_track();
//	_track = _disk->get_track_at_position(0, (unsigned int)_head_position);

	// TODO: probably a better implementation of the empty track?
/*	Time offset;
	if(_track && _time_into_track.length > 0)
	{
		Time time_found = _track->seek_to(_time_into_track).simplify();
		offset = (_time_into_track - time_found).simplify();
		_time_into_track = time_found;
	}
	else
	{
		offset = _time_into_track;
		_time_into_track.set_zero();
	}*/

	reset_timer();
	get_next_event();
//	reset_timer_to_offset(offset * _rotational_multiplier);
}

void Controller::run_for_cycles(int number_of_cycles)
{
	if(_drive && _drive->has_disk() && _motor_is_on)
	{
		if(!_track) setup_track();
		number_of_cycles *= _clock_rate_multiplier;
		while(number_of_cycles)
		{
			int cycles_until_next_event = (int)get_cycles_until_next_event();
			int cycles_to_run_for = std::min(cycles_until_next_event, number_of_cycles);
			_cycles_since_index_hole += (unsigned int)cycles_to_run_for;
			number_of_cycles -= cycles_to_run_for;
			_pll->run_for_cycles(cycles_to_run_for);
			TimedEventLoop::run_for_cycles(cycles_to_run_for);
		}
	}
}

#pragma mark - Track timed event loop

void Controller::get_next_event()
{
	if(_track)
		_current_event = _track->get_next_event();
	else
	{
		_current_event.length.length = 1;
		_current_event.length.clock_rate = 1;
		_current_event.type = Track::Event::IndexHole;
	}

	// divide interval, which is in terms of a rotation of the disk, by rotation speed, and
	// convert it into revolutions per second
	set_next_event_time_interval(_current_event.length * _rotational_multiplier);
}

void Controller::process_next_event()
{
	switch(_current_event.type)
	{
		case Track::Event::FluxTransition:
			_pll->add_pulse();
			_time_into_track += _current_event.length;
		break;
		case Track::Event::IndexHole:
			_cycles_since_index_hole = 0;
			_time_into_track.set_zero();
			process_index_hole();
		break;
	}
	get_next_event();
}

#pragma mark - PLL control and delegate

void Controller::set_expected_bit_length(Time bit_length)
{
	_bit_length = bit_length;

	// this conversion doesn't need to be exact because there's a lot of variation to be taken
	// account of in rotation speed, air turbulence, etc, so a direct conversion will do
	int clocks_per_bit = (int)((bit_length.length * _clock_rate) / bit_length.clock_rate);
	_pll.reset(new DigitalPhaseLockedLoop(clocks_per_bit, clocks_per_bit / 5, 3));
	_pll->set_delegate(this);
}

void Controller::digital_phase_locked_loop_output_bit(int value)
{
	process_input_bit(value, _cycles_since_index_hole);
}

#pragma mark - Drive actions

bool Controller::get_is_track_zero()
{
	if(!_drive) return false;
	return _drive->get_is_track_zero();
}

bool Controller::get_drive_is_ready()
{
	if(!_drive) return false;
	return _drive->has_disk();
}

void Controller::step(int direction)
{
	if(_drive) _drive->step(direction);
	invalidate_track();
}

void Controller::set_motor_on(bool motor_on)
{
	_motor_is_on = motor_on;
}

bool Controller::get_motor_on()
{
	return _motor_is_on;
}

void Controller::set_drive(std::shared_ptr<Drive> drive)
{
	_drive = drive;
	invalidate_track();
}

void Controller::invalidate_track()
{
	_track = nullptr;
}
