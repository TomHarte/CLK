//
//  DigitalPhaseLockedLoop.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 11/07/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#pragma once

#include <array>
#include <cassert>
#include <memory>
#include <vector>

#include "../../../ClockReceiver/ClockReceiver.hpp"

namespace Storage {

/*!
	Template parameters:

	@c bit_handler A class that must implement a method, digital_phase_locked_loop_output_bit(int) for receving bits from the DPLL.
	@c length_of_history The number of historic pulses to consider in locking to phase.
*/
template <typename BitHandler, size_t length_of_history = 3> class DigitalPhaseLockedLoop {
	public:
		/*!
			Instantiates a @c DigitalPhaseLockedLoop.

			@param clocks_per_bit The expected number of cycles between each bit of input.
		*/
		DigitalPhaseLockedLoop(int clocks_per_bit, BitHandler &handler) :
			bit_handler_(handler), window_length_(clocks_per_bit), clocks_per_bit_(clocks_per_bit) {}

		/*!
			Changes the expected window length.
		*/
		void set_clocks_per_bit(int clocks_per_bit) {
			window_length_ = clocks_per_bit_ = clocks_per_bit;
		}

		/*!
			Runs the loop, impliedly posting no pulses during that period.

			@c number_of_cycles The time to run the loop for.
		*/
		void run_for(const Cycles cycles) {
			offset_ += cycles.as_integral();
			phase_ += cycles.as_integral();
			if(phase_ >= window_length_) {
				auto windows_crossed = phase_ / window_length_;

				// Check whether this triggers any 0s.
				if(window_was_filled_) --windows_crossed;
				for(int c = 0; c < windows_crossed; c++)
					bit_handler_.digital_phase_locked_loop_output_bit(0);

				window_was_filled_ = false;
				phase_ %= window_length_;
			}
		}

		/*!
			Announces a pulse at the current time.
		*/
		void add_pulse() {
			if(!window_was_filled_) {
				bit_handler_.digital_phase_locked_loop_output_bit(1);
				window_was_filled_ = true;
				post_phase_offset(phase_, offset_);
				offset_ = 0;
			}
		}

	private:
		BitHandler &bit_handler_;

		void post_phase_offset(Cycles::IntType new_phase, Cycles::IntType new_offset) {
			// Erase the effect of whatever is currently in this slot.
			total_divisor_ -= offset_history_[offset_history_pointer_].divisor;
			total_spacing_ -= offset_history_[offset_history_pointer_].spacing;

			// Fill in the new fields.
			const auto multiple = std::max((new_offset + (clocks_per_bit_ >> 1)) / clocks_per_bit_, Cycles::IntType(1));
			offset_history_[offset_history_pointer_].divisor = multiple;
			offset_history_[offset_history_pointer_].spacing = new_offset;

			// Add in the new values;
			total_divisor_ += offset_history_[offset_history_pointer_].divisor;
			total_spacing_ += offset_history_[offset_history_pointer_].spacing;

			// Advance the write slot.
			offset_history_pointer_ = (offset_history_pointer_ + 1) % offset_history_.size();

#ifndef NDEBUG
			Cycles::IntType td = 0, ts = 0;
			for(auto offset: offset_history_) {
				td += offset.divisor;
				ts += offset.spacing;
			}
			assert(ts == total_spacing_);
			assert(td == total_divisor_);
#endif

			// In net: use an unweighted average of the stored offsets to compute current window size,
			// bucketing them by rounding to the nearest multiple of the base clocks per bit
			window_length_ = std::max(total_spacing_ / total_divisor_, Cycles::IntType(1));

			// Also apply a difference to phase, use a simple spring mechanism as a lowpass filter.
			const auto error = new_phase - (window_length_ >> 1);
			phase_ -= (error + 1) >> 1;
		}

		struct LoggedOffset {
			Cycles::IntType divisor = 1, spacing = 1;
		};
		std::array<LoggedOffset, length_of_history> offset_history_;
		std::size_t offset_history_pointer_ = 0;

		Cycles::IntType total_spacing_ = length_of_history;
		Cycles::IntType total_divisor_ = length_of_history;

		Cycles::IntType phase_ = 0;
		Cycles::IntType window_length_ = 0;

		Cycles::IntType offset_ = 0;
		bool window_was_filled_ = false;

		int clocks_per_bit_ = 0;
};

}
