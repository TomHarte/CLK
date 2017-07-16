//
//  DigitalPhaseLockedLoop.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 11/07/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef DigitalPhaseLockedLoop_hpp
#define DigitalPhaseLockedLoop_hpp

#include <memory>
#include <vector>

namespace Storage {

class DigitalPhaseLockedLoop {
	public:
		/*!
			Instantiates a @c DigitalPhaseLockedLoop.

			@param clocks_per_bit The expected number of cycles between each bit of input.
			@param tolerance The maximum tolerance for bit windows — extremes will be clocks_per_bit ± tolerance.
			@param length_of_history The number of historic pulses to consider in locking to phase.
		*/
		DigitalPhaseLockedLoop(int clocks_per_bit, int tolerance, size_t length_of_history);

		/*!
			Runs the loop, impliedly posting no pulses during that period.

			@c number_of_cycles The time to run the loop for.
		*/
		void run_for_cycles(int number_of_cycles);

		/*!
			Announces a pulse at the current time.
		*/
		void add_pulse();

		/*!
			A receiver for PCM output data; called upon every recognised bit.
		*/
		class Delegate {
			public:
				virtual void digital_phase_locked_loop_output_bit(int value) = 0;
		};
		void set_delegate(Delegate *delegate) {
			delegate_ = delegate;
		}

	private:
		Delegate *delegate_;

		void post_phase_offset(int phase, int offset);
		std::vector<int> phase_error_history_;
		size_t phase_error_pointer_;

		std::vector<int> offset_history_;
		int offset_;

		int phase_;
		int window_length_;
		bool window_was_filled_;

		int clocks_per_bit_;
		int tolerance_;
};

}

#endif /* DigitalPhaseLockedLoop_hpp */
