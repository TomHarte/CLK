//
//  DigitalPhaseLockedLoop.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 11/07/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "DigitalPhaseLockedLoop.hpp"
#include <algorithm>
#include <cstdlib>

using namespace Storage;

DigitalPhaseLockedLoop::DigitalPhaseLockedLoop(int clocks_per_bit, std::size_t length_of_history) :
		offset_history_(length_of_history, 0),
		window_length_(clocks_per_bit),
		clocks_per_bit_(clocks_per_bit) {}

void DigitalPhaseLockedLoop::run_for(const Cycles cycles) {
	offset_ += cycles.as_integral();
	phase_ += cycles.as_integral();
	if(phase_ >= window_length_) {
		auto windows_crossed = phase_ / window_length_;

		// check whether this triggers any 0s, if anybody cares
		if(delegate_) {
			if(window_was_filled_) --windows_crossed;
			for(int c = 0; c < windows_crossed; c++)
				delegate_->digital_phase_locked_loop_output_bit(0);
		}

		window_was_filled_ = false;
		phase_ %= window_length_;
	}
}

void DigitalPhaseLockedLoop::add_pulse() {
	if(!window_was_filled_) {
		if(delegate_) delegate_->digital_phase_locked_loop_output_bit(1);
		window_was_filled_ = true;
		post_phase_offset(phase_, offset_);
		offset_ = 0;
	}
}

void DigitalPhaseLockedLoop::post_phase_offset(Cycles::IntType new_phase, Cycles::IntType new_offset) {
	offset_history_[offset_history_pointer_] = new_offset;
	offset_history_pointer_ = (offset_history_pointer_ + 1) % offset_history_.size();

	// use an unweighted average of the stored offsets to compute current window size,
	// bucketing them by rounding to the nearest multiple of the base clocks per bit
	Cycles::IntType total_spacing = 0;
	Cycles::IntType total_divisor = 0;
	for(auto offset : offset_history_) {
		auto multiple = (offset + (clocks_per_bit_ >> 1)) / clocks_per_bit_;
		if(!multiple) continue;
		total_divisor += multiple;
		total_spacing += offset;
	}
	if(total_divisor) {
		window_length_ = total_spacing / total_divisor;
	}

	auto error = new_phase - (window_length_ >> 1);

	// use a simple spring mechanism as a lowpass filter for phase
	phase_ -= (error + 1) >> 1;
}
