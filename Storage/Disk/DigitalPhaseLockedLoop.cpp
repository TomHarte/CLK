//
//  DigitalPhaseLockedLoop.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 11/07/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "DigitalPhaseLockedLoop.hpp"
#include <algorithm>
#include <cstdlib>

using namespace Storage;

DigitalPhaseLockedLoop::DigitalPhaseLockedLoop(int clocks_per_bit, int tolerance, size_t length_of_history) :
		clocks_per_bit_(clocks_per_bit),
		tolerance_(tolerance),
		phase_(0),
		window_length_(clocks_per_bit),
		phase_error_pointer_(0) {
	phase_error_history_.reset(new std::vector<int>(length_of_history, 0));
}

void DigitalPhaseLockedLoop::run_for_cycles(int number_of_cycles) {
	phase_ += number_of_cycles;
	if(phase_ >= window_length_) {
		int windows_crossed = phase_ / window_length_;

		// check whether this triggers any 0s, if anybody cares
		if(delegate_) {
			if(window_was_filled_) windows_crossed--;
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
		post_phase_error(phase_ - (window_length_ >> 1));
	}
}

void DigitalPhaseLockedLoop::post_phase_error(int error) {
	// use a simple spring mechanism as a lowpass filter for phase
	phase_ -= (error + 1) >> 1;

	// use the average of the last few errors to affect frequency
	std::vector<int> *phase_error_history = phase_error_history_.get();
	size_t phase_error_history_size = phase_error_history->size();

	(*phase_error_history)[phase_error_pointer_] = error;
	phase_error_pointer_ = (phase_error_pointer_ + 1)%phase_error_history_size;

	int total_error = 0;
	for(size_t c = 0; c < phase_error_history_size; c++) {
		total_error += (*phase_error_history)[c];
	}
	int denominator = (int)(phase_error_history_size * 4);
	window_length_ += (total_error + (denominator >> 1)) / denominator;
	window_length_ = std::max(std::min(window_length_, clocks_per_bit_ + tolerance_), clocks_per_bit_ - tolerance_);
}
