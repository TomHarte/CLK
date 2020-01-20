//
//  Tape.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/01/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "Tape.hpp"

using namespace Storage::Tape;

// MARK: - Lifecycle

TapePlayer::TapePlayer(int input_clock_rate) :
	TimedEventLoop(input_clock_rate)
{}

// MARK: - Seeking

void Storage::Tape::Tape::seek(Time &seek_time) {
	Time next_time(0);
	reset();
	while(next_time <= seek_time) {
		get_next_pulse();
		next_time += pulse_.length;
	}
}

Storage::Time Tape::get_current_time() {
	Time time(0);
	uint64_t steps = get_offset();
	reset();
	while(steps--) {
		get_next_pulse();
		time += pulse_.length;
	}
	return time;
}

void Storage::Tape::Tape::reset() {
	offset_ = 0;
	virtual_reset();
}

Tape::Pulse Tape::get_next_pulse() {
	pulse_ = virtual_get_next_pulse();
	offset_++;
	return pulse_;
}

uint64_t Tape::get_offset() {
	return offset_;
}

void Tape::set_offset(uint64_t offset) {
	if(offset == offset_) return;
	if(offset < offset_) {
		reset();
	}
	offset -= offset_;
	while(offset--) get_next_pulse();
}

// MARK: - Player

ClockingHint::Preference TapePlayer::preferred_clocking() {
	return (!tape_ || tape_->is_at_end()) ? ClockingHint::Preference::None : ClockingHint::Preference::JustInTime;
}

void TapePlayer::set_tape(std::shared_ptr<Storage::Tape::Tape> tape) {
	tape_ = tape;
	reset_timer();
	get_next_pulse();
	update_clocking_observer();
}

std::shared_ptr<Storage::Tape::Tape> TapePlayer::get_tape() {
	return tape_;
}

bool TapePlayer::has_tape() {
	return static_cast<bool>(tape_);
}

void TapePlayer::get_next_pulse() {
	// get the new pulse
	if(tape_) {
		current_pulse_ = tape_->get_next_pulse();
		if(tape_->is_at_end()) update_clocking_observer();
	} else {
		current_pulse_.length.length = 1;
		current_pulse_.length.clock_rate = 1;
		current_pulse_.type = Tape::Pulse::Zero;
	}

	set_next_event_time_interval(current_pulse_.length);
}

void TapePlayer::run_for(const Cycles cycles) {
	if(has_tape()) {
		TimedEventLoop::run_for(cycles);
	}
}

void TapePlayer::run_for_input_pulse() {
	jump_to_next_event();
}

void TapePlayer::process_next_event() {
	process_input_pulse(current_pulse_);
	get_next_pulse();
}

// MARK: - Binary Player

BinaryTapePlayer::BinaryTapePlayer(int input_clock_rate) :
	TapePlayer(input_clock_rate)
{}

ClockingHint::Preference BinaryTapePlayer::preferred_clocking() {
	if(!motor_is_running_) return ClockingHint::Preference::None;
	return TapePlayer::preferred_clocking();
}

void BinaryTapePlayer::set_motor_control(bool enabled) {
	if(motor_is_running_ != enabled) {
		motor_is_running_ = enabled;
		update_clocking_observer();
	}
}

bool BinaryTapePlayer::get_motor_control() const {
	return motor_is_running_;
}

void BinaryTapePlayer::set_tape_output(bool set) {
	// TODO
}

bool BinaryTapePlayer::get_input() const {
	return motor_is_running_ && input_level_;
}

void BinaryTapePlayer::run_for(const Cycles cycles) {
	if(motor_is_running_) TapePlayer::run_for(cycles);
}

void BinaryTapePlayer::set_delegate(Delegate *delegate) {
	delegate_ = delegate;
}

void BinaryTapePlayer::process_input_pulse(const Storage::Tape::Tape::Pulse &pulse) {
	bool new_input_level = pulse.type == Tape::Pulse::High;
	if(input_level_ != new_input_level) {
		input_level_ = new_input_level;
		if(delegate_) delegate_->tape_did_change_input(this);
	}
}
