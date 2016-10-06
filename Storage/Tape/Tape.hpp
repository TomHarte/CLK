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
namespace Tape {

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
			enum Type {
				High, Low, Zero
			} type;
			Time length;

			Pulse(Type type, Time length) : type(type), length(length) {}
			Pulse() {}
		};

		/*!
			If at the start of the tape returns the first stored pulse. Otherwise advances past
			the last-returned pulse and returns the next.

			@returns the pulse that begins at the current cursor position.
		*/
		Pulse get_next_pulse();

		/// Returns the tape to the beginning.
		void reset();

		/// @returns @c true if the tape has progressed beyond all recorded content; @c false otherwise.
		virtual bool is_at_end() = 0;

		/// @returns the amount of time preceeding the most recently-returned pulse.
		virtual Time get_current_time() { return _current_time; }

		/// Advances or reverses the tape to the last time before or at @c time from which a pulse starts.
		virtual void seek(Time &time);

	private:
		Time _current_time, _next_time;

		virtual Pulse virtual_get_next_pulse() = 0;
		virtual void virtual_reset() = 0;
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

		void set_tape(std::shared_ptr<Storage::Tape::Tape> tape);
		bool has_tape();
		std::shared_ptr<Storage::Tape::Tape> get_tape();

		void run_for_cycles(int number_of_cycles);
		void run_for_input_pulse();

	protected:
		virtual void process_next_event();
		virtual void process_input_pulse(Tape::Pulse pulse) = 0;

	private:
		inline void get_next_pulse();

		std::shared_ptr<Storage::Tape::Tape> _tape;
		Tape::Pulse _current_pulse;
};

}
}

#endif /* Tape_hpp */
