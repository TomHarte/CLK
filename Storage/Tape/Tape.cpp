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

TapePlayer::TapePlayer(const int input_clock_rate) :
	TimedEventLoop(input_clock_rate)
{}

TapeSerialiser::TapeSerialiser(std::unique_ptr<FormatSerialiser> &&serialiser) : serialiser_(std::move(serialiser)) {}

std::unique_ptr<TapeSerialiser> Tape::serialiser(const TargetPlatform::Type platform) const {
	auto serialiser = format_serialiser();
	if(auto *recipient = dynamic_cast<TargetPlatform::Recipient *>(serialiser.get())) {
		recipient->set_target_platforms(platform);
	}

	return std::make_unique<TapeSerialiser>(std::move(serialiser));
}


// MARK: - Seeking

void TapeSerialiser::seek(const Time seek_time) {
	Time next_time(0);
	reset();
	while(next_time <= seek_time) {
		next_pulse();
		next_time += pulse_.length;
	}
}

Storage::Time TapeSerialiser::current_time() {
	Time time(0);
	uint64_t steps = offset();
	reset();
	while(steps--) {
		next_pulse();
		time += pulse_.length;
	}
	return time;
}

void TapeSerialiser::reset() {
	offset_ = 0;
	serialiser_->reset();
}

Pulse TapeSerialiser::next_pulse() {
	pulse_ = serialiser_->next_pulse();
	offset_++;
	return pulse_;
}

uint64_t TapeSerialiser::offset() const {
	return offset_;
}

void TapeSerialiser::set_offset(uint64_t offset) {
	if(offset == offset_) return;
	if(offset < offset_) {
		reset();
	}
	offset -= offset_;
	while(offset--) next_pulse();
}

bool TapeSerialiser::is_at_end() const {
	return serialiser_->is_at_end();
}

// MARK: - Player

ClockingHint::Preference TapePlayer::preferred_clocking() const {
	return (!tape_ || serialiser_->is_at_end()) ? ClockingHint::Preference::None : ClockingHint::Preference::JustInTime;
}

void TapePlayer::set_tape(std::shared_ptr<Storage::Tape::Tape> tape, TargetPlatform::Type platform) {
	tape_ = tape;
	serialiser_ = tape->serialiser(platform);

	reset_timer();
	next_pulse();
	update_clocking_observer();
}

bool TapePlayer::is_at_end() const {
	return !serialiser_ || serialiser_->is_at_end();
}

TapeSerialiser *TapePlayer::serialiser() {
	return serialiser_.get();
}

bool TapePlayer::has_tape() const {
	return bool(tape_);
}

void TapePlayer::next_pulse() {
	// get the new pulse
	if(tape_) {
		current_pulse_ = serialiser_->next_pulse();
		if(serialiser_->is_at_end()) update_clocking_observer();
	} else {
		current_pulse_.length.length = 1;
		current_pulse_.length.clock_rate = 1;
		current_pulse_.type = Pulse::Zero;
	}

	set_next_event_time_interval(current_pulse_.length);
}

Pulse TapePlayer::current_pulse() const {
	return current_pulse_;
}

void TapePlayer::complete_pulse() {
	jump_to_next_event();
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
	process(current_pulse_);
	next_pulse();
}

// MARK: - Binary Player

BinaryTapePlayer::BinaryTapePlayer(const int input_clock_rate) :
	TapePlayer(input_clock_rate)
{}

ClockingHint::Preference BinaryTapePlayer::preferred_clocking() const {
	if(!motor_is_running_) return ClockingHint::Preference::None;
	return TapePlayer::preferred_clocking();
}

void BinaryTapePlayer::set_motor_control(const bool enabled) {
	if(motor_is_running_ != enabled) {
		motor_is_running_ = enabled;
		update_clocking_observer();

		if(observer_) {
			observer_->set_led_status("Tape motor", enabled);
		}
	}
}

void BinaryTapePlayer::set_activity_observer(Activity::Observer *const observer) {
	observer_ = observer;
	if(observer) {
		observer->register_led("Tape motor");
		observer_->set_led_status("Tape motor", motor_is_running_);
	}
}

bool BinaryTapePlayer::motor_control() const {
	return motor_is_running_;
}

void BinaryTapePlayer::set_tape_output(bool) {
	// TODO
}

bool BinaryTapePlayer::input() const {
	return motor_is_running_ && input_level_;
}

void BinaryTapePlayer::run_for(const Cycles cycles) {
	if(motor_is_running_) TapePlayer::run_for(cycles);
}

void BinaryTapePlayer::set_delegate(Delegate *const delegate) {
	delegate_ = delegate;
}

void BinaryTapePlayer::process(const Storage::Tape::Pulse &pulse) {
	bool new_input_level = pulse.type == Pulse::High;
	if(input_level_ != new_input_level) {
		input_level_ = new_input_level;
		if(delegate_) delegate_->tape_did_change_input(*this);
	}
}
