//
//  DigitalPhaseLockedLoop.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 11/07/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef DigitalPhaseLockedLoop_hpp
#define DigitalPhaseLockedLoop_hpp

namespace Storage {

class DigitalPhaseLockedLoop {
	public:
		/*!
			Instantiates a @c DigitalPhaseLockedLoop.

			@param clocks_per_bit The expected number of cycles between each bit of input.
			@param tolerance The maximum tolerance for bit windows — extremes will be clocks_per_bit ± tolerance.
		*/
		DigitalPhaseLockedLoop(unsigned int clocks_per_bit, unsigned int tolerance);

		/*!
			Runs the loop, impliedly posting no pulses during that period.

			@c number_of_cycles The time to run the loop for.
		*/
		void run_for_cycles(unsigned int number_of_cycles);

		/*!
			Announces a pulse at the current time.
		*/
		void add_pulse();

		/*!
			A receiver for PCM output data; called upon every recognised bit.
		*/
		class Delegate {
			public:
				virtual void digital_phase_locked_loop_output_bit(bool value) = 0;
		};
		void set_delegate(Delegate *delegate)
		{
			_delegate = delegate;
		}

	private:
		Delegate *_delegate;
};

}

#endif /* DigitalPhaseLockedLoop_hpp */
