//
//  CRTMachine.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 31/05/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef CRTMachine_hpp
#define CRTMachine_hpp

#include "../Outputs/CRT/CRT.hpp"
#include "../Outputs/Speaker/Speaker.hpp"
#include "../ClockReceiver/ClockReceiver.hpp"
#include "../ClockReceiver/TimeTypes.hpp"
#include "ROMMachine.hpp"

#include <cmath>

namespace CRTMachine {

/*!
	A CRTMachine::Machine is a mostly-abstract base class for machines that connect to a CRT,
	that optionally provide a speaker, and that nominate a clock rate and can announce to a delegate
	should that clock rate change.
*/
class Machine: public ROMMachine::Machine {
	public:
		/*!
			Causes the machine to set up its CRT and, if it has one, speaker. The caller guarantees
			that an OpenGL context is bound.
		*/
		virtual void setup_output(float aspect_ratio) = 0;

		/*!
			Gives the machine a chance to release all owned resources. The caller guarantees that the
			OpenGL context is bound.
		*/
		virtual void close_output() = 0;

		/// @returns The CRT this machine is drawing to. Should not be @c nullptr.
		virtual Outputs::CRT::CRT *get_crt() = 0;

		/// @returns The speaker that receives this machine's output, or @c nullptr if this machine is mute.
		virtual Outputs::Speaker::Speaker *get_speaker() = 0;

		/// @returns The confidence that this machine is running content it understands.
		virtual float get_confidence() { return 0.5f; }
		virtual void print_type() {}

		/// Runs the machine for @c duration seconds.
		virtual void run_for(Time::Seconds duration) {
			const double cycles = (duration * clock_rate_) + clock_conversion_error_;
			clock_conversion_error_ = std::fmod(cycles, 1.0);
			run_for(Cycles(static_cast<int>(cycles)));
		}

		double get_clock_rate() {
			return clock_rate_;
		}

	protected:
		/// Runs the machine for @c cycles.
		virtual void run_for(const Cycles cycles) = 0;
		void set_clock_rate(double clock_rate) {
			clock_rate_ = clock_rate;
		}

	private:
		double clock_rate_ = 1.0;
		double clock_conversion_error_ = 0.0;
};

}

#endif /* CRTMachine_hpp */
