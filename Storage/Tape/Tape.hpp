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
#include "../TimedEventLoop.hpp"

namespace Storage {

/*!
	Models a tape as a sequence of pulses, each pulse being of arbitrary length and described
	by their relationship with zero:
		- high pulses exit from zero upward before returning to it;
		- low pulses exit from zero downward before returning to it;
		- zero pulses run along zero.
		
	Subclasses should implement at least @c get_next_pulse and @c reset to provide a serial feeding
	of pulses and the ability to return to the start of the feed. They may also implement @c seek if
	a better implementation than a linear search from the @c reset time can be implemented.
*/
class Tape {
	public:
		struct Pulse {
			enum {
				High, Low, Zero
			} type;
			Time length;
		};

		virtual Pulse get_next_pulse() = 0;
		virtual void reset() = 0;

		virtual void seek(Time seek_time);	// TODO
};

/*!
	Provides a helper for: (i) retaining a reference to a tape; and (ii) running the tape at a certain
	input clock rate.

	Will call @c process_input_pulse instantaneously upon reaching *the end* of a pulse. Therefore a subclass
	can decode pulses into data within process_input_pulse, using the supplied pulse's @c length and @c type.
*/
class TapePlayer: public TimedEventLoop {
	public:
		TapePlayer(unsigned int input_clock_rate);

		void set_tape(std::shared_ptr<Storage::Tape> tape);
		bool has_tape();

		void run_for_cycles(int number_of_cycles);
		void run_for_input_pulse();

	protected:
		virtual void process_next_event();
		virtual void process_input_pulse(Tape::Pulse pulse) = 0;

	private:
		inline void get_next_pulse();

		std::shared_ptr<Storage::Tape> _tape;
		Tape::Pulse _current_pulse;
};

}

#endif /* Tape_hpp */
