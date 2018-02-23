//
//  PulseQueuedTape.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/07/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef PulseQueuedTape_hpp
#define PulseQueuedTape_hpp

#include "Tape.hpp"
#include <vector>

namespace Storage {
namespace Tape {

/*!
	Provides a @c Tape with a queue of upcoming pulses and an is-at-end flag.

	If is-at-end is set then get_next_pulse() returns a second of silence and
	is_at_end() returns true.

	Otherwise get_next_pulse() returns something from the pulse queue if there is
	anything there, and otherwise calls get_next_pulses(). get_next_pulses() is
	virtual, giving subclasses a chance to provide the next batch of pulses.
*/
class PulseQueuedTape: public Tape {
	public:
		PulseQueuedTape();
		bool is_at_end();

	protected:
		void emplace_back(Tape::Pulse::Type type, Time length);
		void emplace_back(const Tape::Pulse &&pulse);
		void clear();
		bool empty();

		void set_is_at_end(bool);
		virtual void get_next_pulses() = 0;

	private:
		Pulse virtual_get_next_pulse();
		Pulse silence();

		std::vector<Pulse> queued_pulses_;
		std::size_t pulse_pointer_;
		bool is_at_end_;
};

}
}

#endif /* PulseQueuedTape_hpp */
