//
//  Tape.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/01/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#pragma once

#include "../../ClockReceiver/ClockReceiver.hpp"
#include "../../ClockReceiver/ClockingHintSource.hpp"

#include "../TimedEventLoop.hpp"

#include "../../Activity/Observer.hpp"
#include "../TargetPlatforms.hpp"

#include <memory>

namespace Storage::Tape {

struct Pulse {
	enum Type {
		High, Low, Zero
	} type;
	Time length;

	Pulse(Type type, Time length) : type(type), length(length) {}
	Pulse() = default;
};

/*!
	Provdes the means for tape serialiserion.
*/
class FormatSerialiser {
public:
	virtual ~FormatSerialiser() = default;
	virtual Pulse next_pulse() = 0;
	virtual void reset() = 0;
	virtual bool is_at_end() const = 0;
};

class TapeSerialiser {
public:
	TapeSerialiser(std::unique_ptr<FormatSerialiser> &&);
	virtual ~TapeSerialiser() = default;

	/*!
		If at the start of the tape returns the first stored pulse. Otherwise advances past
		the last-returned pulse and returns the next.

		@returns the pulse that begins at the current cursor position.
	*/
	Pulse next_pulse();

	/// Returns the tape to the beginning.
	void reset();

	/// @returns @c true if the tape has progressed beyond all recorded content; @c false otherwise.
	bool is_at_end() const;

	/*!
		Returns a numerical representation of progression into the tape. Precision is arbitrary but
		required to be at least to the whole pulse. Greater numbers are later than earlier numbers,
		but not necessarily continuous.
	*/
	uint64_t offset() const;

	/*!
		Moves the tape to the first time at which the specified offset would be returned by get_offset.
	*/
	void set_offset(uint64_t);

	/*!
		Calculates and returns the amount of time that has elapsed since the time began. Potentially expensive.
	*/
	Time current_time();

	/*!
		Seeks to @c time. Potentially expensive.
	*/
	void seek(Time);

private:
	uint64_t offset_{};
	Pulse pulse_;
	std::unique_ptr<FormatSerialiser> &serialiser_;
};

/*!
	Models a tape as a sequence of pulses, each pulse being of arbitrary length and described
	by their relationship with zero:
		- high pulses exit from zero upward before returning to it;
		- low pulses exit from zero downward before returning to it;
		- zero pulses run along zero.
*/
class Tape {
public:
	std::unique_ptr<TapeSerialiser> serialiser() const;

protected:
	virtual std::unique_ptr<FormatSerialiser> format_serialiser() const = 0;
};

/*!
	Provides a helper for: (i) retaining a reference to a tape; and (ii) running the tape at a certain
	input clock rate.

	Will call @c process(Pulse) instantaneously upon reaching *the end* of a pulse. Therefore a subclass
	can decode pulses into data within @c process, using the supplied pulse's @c length and @c type.
*/
class TapePlayer: public TimedEventLoop, public ClockingHint::Source {
public:
	TapePlayer(int input_clock_rate);
	virtual ~TapePlayer() = default;

	void set_tape(std::shared_ptr<Storage::Tape::Tape>);
	bool has_tape() const;
	bool is_at_end() const;
	TapeSerialiser *serialiser();

	void run_for(Cycles);
	void run_for_input_pulse();

	ClockingHint::Preference preferred_clocking() const override;

	Pulse current_pulse() const;
	void complete_pulse();

protected:
	virtual void process_next_event() override;
	virtual void process(const Pulse &) = 0;

private:
	inline void next_pulse();

	std::shared_ptr<Storage::Tape::Tape> tape_;
	std::unique_ptr<TapeSerialiser> serialiser_;
	Pulse current_pulse_;
};

/*!
	A specific subclass of the tape player for machines that sample such as to report only either a
	high or a low current input level.

	Such machines can use @c get_input() to get the current level of the input.

	They can also provide a delegate to be notified upon any change in the input level.
*/
class BinaryTapePlayer : public TapePlayer {
public:
	BinaryTapePlayer(int input_clock_rate);
	void set_motor_control(bool);
	bool motor_control() const;

	void set_tape_output(bool);
	bool input() const;

	void run_for(Cycles);

	struct Delegate {
		virtual void tape_did_change_input(BinaryTapePlayer *) = 0;
	};
	void set_delegate(Delegate *);

	ClockingHint::Preference preferred_clocking() const final;

	void set_activity_observer(Activity::Observer *);

protected:
	Delegate *delegate_ = nullptr;
	void process(const Pulse &) final;
	bool input_level_ = false;
	bool motor_is_running_ = false;

	Activity::Observer *observer_ = nullptr;
};

}
