//
//  MFP68901.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/10/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef MFP68901_hpp
#define MFP68901_hpp

#include <cstdint>
#include "../../ClockReceiver/ClockReceiver.hpp"

namespace Motorola {
namespace MFP68901 {

class MFP68901 {
	public:
		uint8_t read(int address);
		void write(int address, uint8_t value);

		void run_for(HalfCycles);
		HalfCycles get_next_sequence_point();

		void set_timer_event_input(int channel, bool value);

	private:
		// MARK: - Timers
		enum class TimerMode {
			Stopped, EventCount, Delay, PulseWidth
		};
		void set_timer_mode(int timer, TimerMode, int prescale, bool reset_timer);
		void set_timer_data(int timer, uint8_t);
		uint8_t get_timer_data(int timer);
		void decrement_timer(int timer);

		struct Timer {
			TimerMode mode = TimerMode::Stopped;
			uint8_t value = 0;
			uint8_t reload_value = 0;
			int prescale = 1;
			int divisor = 0;
			bool event_input = false;
		} timers_[4];

		HalfCycles cycles_left_;
};

}
}

#endif /* MFP68901_hpp */
