//
//  DigitalPhaseLockedLoop.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 11/07/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#ifndef DigitalPhaseLockedLoop_hpp
#define DigitalPhaseLockedLoop_hpp

#include <memory>
#include <vector>

#include "../../../ClockReceiver/ClockReceiver.hpp"

namespace Storage {

class DigitalPhaseLockedLoop {
	public:
		/*!
			Instantiates a @c DigitalPhaseLockedLoop.

			@param clocks_per_bit The expected number of cycles between each bit of input.
			@param length_of_history The number of historic pulses to consider in locking to phase.
		*/
		DigitalPhaseLockedLoop(int clocks_per_bit, std::size_t length_of_history);

		/*!
			Runs the loop, impliedly posting no pulses during that period.

			@c number_of_cycles The time to run the loop for.
		*/
		void run_for(const Cycles cycles);

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
		Delegate *delegate_ = nullptr;

		void post_phase_offset(Cycles::IntType phase, Cycles::IntType offset);

		std::vector<Cycles::IntType> offset_history_;
		std::size_t offset_history_pointer_ = 0;
		Cycles::IntType offset_ = 0;

		Cycles::IntType phase_ = 0;
		Cycles::IntType window_length_ = 0;
		bool window_was_filled_ = false;

		int clocks_per_bit_ = 0;
		int tolerance_ = 0;
};

}

#endif /* DigitalPhaseLockedLoop_hpp */
