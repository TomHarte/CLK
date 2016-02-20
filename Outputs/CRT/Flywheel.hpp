//
//  Flywheel.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 11/02/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Flywheel_hpp
#define Flywheel_hpp

namespace Outputs {

/*!
	Provides timing for a two-phase signal consisting of a retrace phase followed by a scan phase,
	announcing the start and end of retrace and providing the abiliy to read the current
	scanning position.

	The @c Flywheel will attempt to converge with timing implied by synchronisation pulses.
*/
struct Flywheel
{
	/*!
		Constructs an instance of @c Flywheel.

		@param standard_period The expected amount of time between one synchronisation and the next.

		@param retrace_time The amount of time it takes to complete a retrace.
	*/
	Flywheel(unsigned int standard_period, unsigned int retrace_time) :
		_standard_period(standard_period),
		_retrace_time(retrace_time),
		_sync_error_window(standard_period >> 7),
		_counter(0),
		_expected_next_sync(standard_period),
		_counter_before_retrace(standard_period - retrace_time) {}

	enum SyncEvent {
		/// Indicates that no synchronisation events will occur in the queried window.
		None,
		/// Indicates that the next synchronisation event will be a transition into retrce.
		StartRetrace,
		/// Indicates that the next synchronisation event will be a transition out of retrace.
		EndRetrace
	};
	/*!
		Asks the flywheel for the first synchronisation event that will occur in a given time period,
		indicating whether a synchronisation request occurred at the start of the query window.

		@param sync_is_requested @c true indicates that the flywheel should act as though having
		received a synchronisation request now; @c false indicates no such event was detected.

		@param cycles_to_run_for The number of cycles to look ahead.

		@param cycles_advanced After this method has completed, contains the amount of time until
		the returned synchronisation event.

		@returns The next synchronisation event.
	*/
	inline SyncEvent get_next_event_in_period(bool sync_is_requested, unsigned int cycles_to_run_for, unsigned int *cycles_advanced)
	{
		// do we recognise this hsync, thereby adjusting future time expectations?
		if(sync_is_requested)
		{
			if(_counter < _sync_error_window || _counter > _expected_next_sync - _sync_error_window)
			{
				unsigned int time_now = (_counter < _sync_error_window) ? _expected_next_sync + _counter : _counter;
				_expected_next_sync = (_expected_next_sync + _expected_next_sync + _expected_next_sync + time_now) >> 2;
			}
			else
			{
				_number_of_surprises++;

				if(_counter < _retrace_time + (_expected_next_sync >> 1))
				{
					_expected_next_sync++;
				}
				else
				{
					_expected_next_sync--;
				}
			}
		}

		SyncEvent proposed_event = SyncEvent::None;
		unsigned int proposed_sync_time = cycles_to_run_for;

		// will we end an ongoing retrace?
		if(_counter < _retrace_time && _counter + proposed_sync_time >= _retrace_time)
		{
			proposed_sync_time = _retrace_time - _counter;
			proposed_event = SyncEvent::EndRetrace;
		}

		// will we start a retrace?
		if(_counter + proposed_sync_time >= _expected_next_sync)
		{
			proposed_sync_time = _expected_next_sync - _counter;
			proposed_event = SyncEvent::StartRetrace;
		}

		*cycles_advanced = proposed_sync_time;
		return proposed_event;
	}

	/*!
		Advances a nominated amount of time, applying a previously returned synchronisation event
		at the end of that period.

		@param cycles_advanced The amount of time to run for.

		@param event The synchronisation event to apply after that period.
	*/
	inline void apply_event(unsigned int cycles_advanced, SyncEvent event)
	{
		_counter += cycles_advanced;

		switch(event)
		{
			default: return;
			case StartRetrace:
				_counter_before_retrace = _counter - _retrace_time;
				_counter = 0;
			return;
		}
	}

	/*!
		Returns the current output position; while in retrace this will go down towards 0, while in scan
		it will go upward.

		@returns The current output position.
	*/
	inline unsigned int get_current_output_position()
	{
		if(_counter < _retrace_time)
		{
			unsigned int retrace_distance = (_counter * _standard_period) / _retrace_time;
			if(retrace_distance > _counter_before_retrace) return 0;
			return _counter_before_retrace - retrace_distance;
		}

		return _counter - _retrace_time;
	}

	/*!
		@returns the amount of time since retrace last began. Time then counts monotonically up from zero.
	*/
	inline unsigned int get_current_time()
	{
		return _counter;
	}

	/*!
		@returns whether the output is currently retracing.
	*/
	inline bool is_in_retrace()
	{
		return _counter < _retrace_time;
	}

	/*!
		@returns the expected length of the scan period.
	*/
	inline unsigned int get_scan_period()
	{
		return _standard_period - _retrace_time;
	}

	/*!
		@returns the number of synchronisation events that have seemed surprising since the last time this method was called;
		a low number indicates good synchronisation.
	*/
	inline unsigned int get_and_reset_number_of_surprises()
	{
		unsigned int result = _number_of_surprises;
		_number_of_surprises = 0;
		return result;
	}

	private:
		unsigned int _standard_period;			// the normal length of time between syncs
		const unsigned int _retrace_time;		// a constant indicating the amount of time it takes to perform a retrace
		const unsigned int _sync_error_window;	// a constant indicating the window either side of the next expected sync in which we'll accept other syncs

		unsigned int _counter;					// time since the _start_ of the last sync
		unsigned int _counter_before_retrace;	// the value of _counter immediately before retrace began
		unsigned int _expected_next_sync;		// our current expection of when the next sync will be encountered (which implies velocity)

		unsigned int _number_of_surprises;		// a count of the surprising syncs

		/*
			Implementation notes:

			Retrace takes a fixed amount of time and runs during [0, _retrace_time).

			For the current line, scan then occurs from [_retrace_time, _expected_next_sync), at which point
			retrace begins and the internal counter is reset.

			All synchronisation events that occur within (-_sync_error_window, _sync_error_window) of the
			expected synchronisation time will cause a proportional adjustment in the expected time for the next
			synchronisation. Other synchronisation events are clamped as though they occurred in that range.
		*/
};

}

#endif /* Flywheel_hpp */
