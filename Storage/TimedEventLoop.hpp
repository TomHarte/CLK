//
//  TimedEventLoop.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/07/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#ifndef TimedEventLoop_hpp
#define TimedEventLoop_hpp

#include "Storage.hpp"
#include "../ClockReceiver/ClockReceiver.hpp"
#include "../SignalProcessing/Stepper.hpp"

#include <memory>

namespace Storage {

	/*!
		Provides a mechanism for arbitrarily timed events to be processed according to a fixed-base
		discrete clock signal, ensuring correct timing.

		Subclasses are responsible for calling @c set_next_event_time_interval to establish the time
		until a next event; @c process_next_event will be called when that event occurs, with progression
		determined via @c run_for.

		Due to the aggregation of total timing information between events, e.g. if an event loop has
		a clock rate of 1000 ticks per second and a steady stream of events that occur 10,000 times a second,
		bookkeeping is necessary to ensure that 10 events are triggered per tick. Subclasses should call
		@c reset_timer if there is a discontinuity in events.

		Subclasses may also call @c jump_to_next_event to cause the next event to be communicated instantly.

		Subclasses are therefore expected to call @c set_next_event_time_interval upon obtaining an event stream,
		and again in response to each call to @c process_next_event while events are ongoing. They may use
		@c reset_timer to initiate a distinctly-timed stream or @c jump_to_next_event to short-circuit the timing
		loop and fast forward immediately to the next event.
	*/
	class TimedEventLoop {
		public:
			/*!
				Constructs a timed event loop that will be clocked at @c input_clock_rate.
			*/
			TimedEventLoop(Cycles::IntType input_clock_rate);

			/*!
				Advances the event loop by @c number_of_cycles cycles.
			*/
			void run_for(const Cycles cycles);

			/*!
				@returns the number of whole cycles remaining until the next event is triggered.
			*/
			Cycles::IntType get_cycles_until_next_event() const;

			/*!
				@returns the input clock rate.
			*/
			Cycles::IntType get_input_clock_rate() const;

		protected:
			/*!
				Sets the time interval, as a proportion of a second, until the next event should be triggered.
			*/
			void set_next_event_time_interval(Time interval);
			void set_next_event_time_interval(float interval);

			/*!
				Communicates that the next event is triggered. A subclass will idiomatically process that event
				and make a fresh call to @c set_next_event_time_interval to keep the event loop running.
			*/
			virtual void process_next_event() = 0;

			/*!
				Optionally allows a subclass to track time within run_for periods; if a subclass implements
				advnace then it will receive advance increments that add up to the number of cycles supplied
				to run_for, but calls to process_next_event will be precisely interspersed. No time will carry
				forward between calls into run_for; a subclass can receive arbitrarily many instructions to
				advance before receiving a process_next_event.
			*/
			virtual void advance([[maybe_unused]] const Cycles cycles) {};

			/*!
				Resets timing, throwing away any current internal state. So clears any fractional ticks
				that the event loop is currently tracking.
			*/
			void reset_timer();

			/*!
				Causes an immediate call to @c process_next_event and a call to @c reset_timer with the
				net effect of processing the current event immediately and fast forwarding exactly to the
				start of the interval prior to the next event.
			*/
			void jump_to_next_event();

			/*!
				@returns the amount of time that has passed since the last call to @c set_next_time_interval,
				which will always be less than or equal to the time that was supplied to @c set_next_time_interval.
			*/
			Time get_time_into_next_event();

		private:
			Cycles::IntType input_clock_rate_ = 0;
			Cycles::IntType cycles_until_event_ = 0;
			float subcycles_until_event_ = 0.0;
	};

}

#endif /* TimedEventLoop_hpp */
