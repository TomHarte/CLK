//
//  PulseQueuedTape.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/07/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#pragma once

#include "Tape.hpp"
#include <vector>

namespace Storage::Tape {

/*!
	Provides a @c Tape with a queue of upcoming pulses and an is-at-end flag.

	If is-at-end is set then get_next_pulse() returns a second of silence and
	is_at_end() returns true.

	Otherwise get_next_pulse() returns something from the pulse queue if there is
	anything there, and otherwise calls get_next_pulses(). get_next_pulses() is
	virtual, giving subclasses a chance to provide the next batch of pulses.
*/
class PulseQueuedSerialiser: public TapeSerialiser {
public:
	void emplace_back(Pulse::Type, Time);
	void push_back(Pulse);
	void clear();
	bool empty() const;

	void set_is_at_end(bool);
	Pulse get_next_pulse() override;
	bool is_at_end() const override;

	virtual void get_next_pulses() = 0;

private:
	std::vector<Pulse> queued_pulses_;
	std::size_t pulse_pointer_ = 0;
	bool is_at_end_ = false;
};

}
