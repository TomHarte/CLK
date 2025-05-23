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

	If is-at-end is set then @c next_pulse() returns a second of silence and
	@c is_at_end() returns @c true.

	Otherwise @c next_pulse() returns something from the pulse queue if there is
	anything there, and otherwise calls @c push_next_pulses() which is
	virtual, giving subclasses a chance to provide the next batch of pulses.
*/
class PulseQueuedSerialiser: public FormatSerialiser {
public:
	void emplace_back(Pulse::Type, Time);
	void push_back(Pulse);
	void clear();
	bool empty() const;

	void set_is_at_end(bool);
	Pulse next_pulse() override;
	bool is_at_end() const override;

	virtual void push_next_pulses() = 0;

private:
	std::vector<Pulse> queued_pulses_;
	std::size_t pulse_pointer_ = 0;
	bool is_at_end_ = false;
};

}
