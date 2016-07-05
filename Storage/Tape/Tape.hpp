//
//  Tape.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Tape_hpp
#define Tape_hpp

#include <memory>
#include "../../SignalProcessing/Stepper.hpp"

namespace Storage {

class Tape {
	public:
		struct Time {
			unsigned int length, clock_rate;
		};

		struct Pulse {
			enum {
				High, Low, Zero
			} type;
			Time length;
		};

		virtual Pulse get_next_pulse() = 0;
		virtual void reset() = 0;

		virtual void seek(Time seek_time);
};

class TapePlayer {
	public:
		TapePlayer(unsigned int input_clock_rate);

		void set_tape(std::shared_ptr<Storage::Tape> tape);
		bool has_tape();

		void run_for_cycles(unsigned int number_of_cycles);
		void run_for_input_pulse();

	protected:
		virtual void process_input_pulse(Tape::Pulse pulse) = 0;

	private:
		inline void get_next_pulse();

		unsigned int _input_clock_rate;
		std::shared_ptr<Storage::Tape> _tape;
		struct {
			Tape::Pulse current_pulse;
			std::unique_ptr<SignalProcessing::Stepper> pulse_stepper;
			uint32_t time_into_pulse;
		} _input;
};

}

#endif /* Tape_hpp */
