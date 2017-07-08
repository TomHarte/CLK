//
//  Tape.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Tape.hpp"
#include "../../NumberTheory/Factors.hpp"

using namespace Storage::Tape;

#pragma mark - Lifecycle

TapePlayer::TapePlayer(unsigned int input_clock_rate) :
	TimedEventLoop(input_clock_rate)
{}

#pragma mark - Seeking

void Storage::Tape::Tape::seek(Time &seek_time) {
	current_time_.set_zero();
	next_time_.set_zero();
	while(next_time_ <= seek_time) get_next_pulse();
}

void Storage::Tape::Tape::reset() {
	current_time_.set_zero();
	next_time_.set_zero();
	virtual_reset();
}

Tape::Pulse Tape::get_next_pulse() {
	Tape::Pulse pulse = virtual_get_next_pulse();
	current_time_ = next_time_;
	next_time_ += pulse.length;
	return pulse;
}

#pragma mark - Player

void TapePlayer::set_tape(std::shared_ptr<Storage::Tape::Tape> tape) {
	tape_ = tape;
	reset_timer();
	get_next_pulse();
}

std::shared_ptr<Storage::Tape::Tape> TapePlayer::get_tape() {
	return tape_;
}

bool TapePlayer::has_tape() {
	return (bool)tape_;
}

void TapePlayer::get_next_pulse() {
	// get the new pulse
	if(tape_)
		current_pulse_ = tape_->get_next_pulse();
	else {
		current_pulse_.length.length = 1;
		current_pulse_.length.clock_rate = 1;
		current_pulse_.type = Tape::Pulse::Zero;
	}

	set_next_event_time_interval(current_pulse_.length);
}

void TapePlayer::run_for_cycles(int number_of_cycles) {
	if(has_tape()) {
		TimedEventLoop::run_for_cycles(number_of_cycles);
	}
}

void TapePlayer::run_for_input_pulse() {
	jump_to_next_event();
}

void TapePlayer::process_next_event() {
	process_input_pulse(current_pulse_);
	get_next_pulse();
}

#pragma mark - Binary Player

BinaryTapePlayer::BinaryTapePlayer(unsigned int input_clock_rate) :
	TapePlayer(input_clock_rate), motor_is_running_(false)
{}

void BinaryTapePlayer::set_motor_control(bool enabled) {
	motor_is_running_ = enabled;
}

void BinaryTapePlayer::set_tape_output(bool set) {
	// TODO
}

bool BinaryTapePlayer::get_input() {
	return input_level_;
}

void BinaryTapePlayer::run_for_cycles(int number_of_cycles) {
	if(motor_is_running_) TapePlayer::run_for_cycles(number_of_cycles);
}

void BinaryTapePlayer::set_delegate(Delegate *delegate) {
	delegate_ = delegate;
}

void BinaryTapePlayer::process_input_pulse(Storage::Tape::Tape::Pulse pulse) {
	bool new_input_level = pulse.type == Tape::Pulse::High;
	if(input_level_ != new_input_level) {
		input_level_ = new_input_level;
		if(delegate_) delegate_->tape_did_change_input(this);
	}
}
