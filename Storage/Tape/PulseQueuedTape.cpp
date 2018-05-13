//
//  PulseQueuedTape.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/07/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "PulseQueuedTape.hpp"

using namespace Storage::Tape;

PulseQueuedTape::PulseQueuedTape() : pulse_pointer_(0), is_at_end_(false) {}

bool PulseQueuedTape::is_at_end() {
	return is_at_end_;
}

void PulseQueuedTape::set_is_at_end(bool is_at_end) {
	is_at_end_ = is_at_end;
}

void PulseQueuedTape::clear() {
	queued_pulses_.clear();
	pulse_pointer_ = 0;
}

bool PulseQueuedTape::empty() {
	return queued_pulses_.empty();
}

void PulseQueuedTape::emplace_back(Tape::Pulse::Type type, Time length) {
	queued_pulses_.emplace_back(type, length);
}

void PulseQueuedTape::emplace_back(const Tape::Pulse &&pulse) {
	queued_pulses_.emplace_back(pulse);
}

Tape::Pulse PulseQueuedTape::silence() {
	Pulse silence;
	silence.type = Pulse::Zero;
	silence.length.length = 1;
	silence.length.clock_rate = 1;
	return silence;
}

Tape::Pulse PulseQueuedTape::virtual_get_next_pulse() {
	if(is_at_end_) {
		return silence();
	}

	if(pulse_pointer_ == queued_pulses_.size()) {
		clear();
		get_next_pulses();

		if(is_at_end_ || pulse_pointer_ == queued_pulses_.size()) {
			return silence();
		}
	}

	std::size_t read_pointer = pulse_pointer_;
	pulse_pointer_++;
	return queued_pulses_[read_pointer];
}
