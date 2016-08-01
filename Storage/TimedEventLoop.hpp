//
//  TimedEventLoop.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/07/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef TimedEventLoop_hpp
#define TimedEventLoop_hpp

#include "Storage.hpp"

#include <memory>
#include "../SignalProcessing/Stepper.hpp"

namespace Storage {

	/*!
		Provides a mechanism for arbitrarily timed events to be processed according to a fixed-base
		discrete clock signal, ensuring correct timing.

		Subclasses are responsible for calling @c set_next_event_time_interval to establish the time
		until a next event; @c process_next_event will be called when that event occurs, with progression
		determined via @c run_for_cycles.

		Due to the aggregation of total timing information between events — e.g. if an event loop has
		a clock rate of 1000 ticks per second and a steady stream of events that occur 10,000 times a second,
		bookkeeping is necessary to ensure that 10 events are triggered per tick — subclasses should call
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
			TimedEventLoop(unsigned int input_clock_rate);

			/*!
				Advances the event loop by @c number_of_cycles cycles.
			*/
			void run_for_cycles(int number_of_cycles);

		protected:
			/*!
				Sets the time interval, as a proportion of a second, until the next event should be triggered.
			*/
			void set_next_event_time_interval(Time interval);

			/*!
				Communicates that the next event is triggered. A subclass will idiomatically process that event
				and make a fresh call to @c set_next_event_time_interval to keep the event loop running.
			*/
			virtual void process_next_event() = 0;

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

		private:
			unsigned int _input_clock_rate;
			Time _event_interval;
			std::unique_ptr<SignalProcessing::Stepper> _stepper;
			uint32_t _time_into_interval;
	};

}

#endif /* TimedEventLoop_hpp */
