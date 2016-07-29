//
//  DiskDrive.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/07/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "DiskDrive.hpp"

using namespace Storage;

DiskDrive::DiskDrive(unsigned int clock_rate, unsigned int revolutions_per_minute) :
	_clock_rate(clock_rate),
	_revolutions_per_minute(revolutions_per_minute),
	_head_position(0),

	TimedEventLoop(clock_rate)
{}

void DiskDrive::set_expected_bit_length(Time bit_length)
{
	_bit_length = bit_length;

	// this conversion doesn't need to be exact because there's a lot of variation to be taken
	// account of in rotation speed, air turbulence, etc, so a direct conversion will do
	int clocks_per_bit = (int)((bit_length.length * _clock_rate) / bit_length.clock_rate);
	_pll.reset(new DigitalPhaseLockedLoop(clocks_per_bit, clocks_per_bit / 5, 3));
}

void DiskDrive::set_disk(std::shared_ptr<Disk> disk)
{
	_disk = disk;
	set_track();
}

bool DiskDrive::has_disk()
{
	return (bool)_disk;
}

bool DiskDrive::get_is_track_zero()
{
	return _head_position == 0;
}

void DiskDrive::step(int direction)
{
	_head_position = std::max(_head_position + direction, 0);
	set_track();
}

void DiskDrive::set_track()
{
	_track = _disk->get_track_at_position((unsigned int)_head_position);
	reset_timer();
	get_next_event();
}

void DiskDrive::run_for_cycles(unsigned int number_of_cycles)
{
	if(has_disk())
	{
		_cycles_since_index_hole += number_of_cycles;
		TimedEventLoop::run_for_cycles(number_of_cycles);
	}
}

#pragma mark - Track timed event loop

void DiskDrive::get_next_event()
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
	Time event_interval = _current_event.length;
	event_interval.length *= 60;
	event_interval.clock_rate *= _revolutions_per_minute;
	event_interval.simplify();
	set_next_event_time_interval(event_interval);
}

void DiskDrive::process_next_event()
{
	switch(_current_event.type)
	{
		case Track::Event::FluxTransition:
			_pll->add_pulse();
		break;
		case Track::Event::IndexHole:
			_cycles_since_index_hole = 0;
			process_index_hole();
		break;
	}
	get_next_event();
}

#pragma mark - PLL delegate

void DiskDrive::digital_phase_locked_loop_output_bit(int value)
{
	process_input_bit(value, _cycles_since_index_hole);
}
