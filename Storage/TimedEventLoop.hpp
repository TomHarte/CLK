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

	class TimedEventLoop {
		public:
			TimedEventLoop(unsigned int input_clock_rate);
			void run_for_cycles(int number_of_cycles);

		protected:
			void reset_timer();
			void set_next_event_time_interval(Time interval);
			void jump_to_next_event();

			virtual void process_next_event() = 0;

		private:
			unsigned int _input_clock_rate;
			Time _event_interval;
			std::unique_ptr<SignalProcessing::Stepper> _stepper;
			uint32_t _time_into_interval;
	};

}

#endif /* TimedEventLoop_hpp */
