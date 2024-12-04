//
//  PulseQueuedTape.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/07/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "PulseQueuedTape.hpp"

using namespace Storage::Tape;

bool PulseQueuedSerialiser::is_at_end() const {
	return is_at_end_;
}

void PulseQueuedSerialiser::set_is_at_end(const bool is_at_end) {
	is_at_end_ = is_at_end;
}

void PulseQueuedSerialiser::clear() {
	queued_pulses_.clear();
	pulse_pointer_ = 0;
}

bool PulseQueuedSerialiser::empty() const {
	return queued_pulses_.empty();
}

void PulseQueuedSerialiser::emplace_back(const Pulse::Type type, const Time length) {
	queued_pulses_.emplace_back(type, length);
}

void PulseQueuedSerialiser::push_back(const Pulse pulse) {
	queued_pulses_.push_back(pulse);
}

Pulse PulseQueuedSerialiser::next_pulse() {
	const auto silence = [] {
		return Pulse(Pulse::Type::Zero, Storage::Time(1, 1));
	};

	if(is_at_end_) {
		return silence();
	}

	if(pulse_pointer_ == queued_pulses_.size()) {
		clear();
		push_next_pulses();

		if(is_at_end_ || pulse_pointer_ == queued_pulses_.size()) {
			return silence();
		}
	}

	const std::size_t read_pointer = pulse_pointer_;
	pulse_pointer_++;
	return queued_pulses_[read_pointer];
}
