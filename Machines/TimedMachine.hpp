//
//  TimedMachine.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 31/03/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#ifndef TimedMachine_h
#define TimedMachine_h

#include "../ClockReceiver/ClockReceiver.hpp"
#include "../ClockReceiver/TimeTypes.hpp"

#include "AudioProducer.hpp"
#include "ScanProducer.hpp"

#include <cmath>

namespace MachineTypes {

/*!
	A timed machine is any which requires the owner to provide time-based updates,
	i.e. run_for(<some number of seconds>)-type calls.

*/
class TimedMachine {
	public:
		/// Runs the machine for @c duration seconds.
		virtual void run_for(Time::Seconds duration) {
			const double cycles = (duration * clock_rate_ * speed_multiplier_) + clock_conversion_error_;
			clock_conversion_error_ = std::fmod(cycles, 1.0);
			run_for(Cycles(int(cycles)));
		}

		/*!
			Sets a speed multiplier to apply to this machine; e.g. a multiplier of 1.5 will cause the
			emulated machine to run 50% faster than a real machine. This speed-up is an emulation
			fiction: it will apply across the system, including to the CRT.
		*/
		virtual void set_speed_multiplier(double multiplier) {
			speed_multiplier_ = multiplier;

			auto audio_producer = dynamic_cast<AudioProducer *>(this);
			if(!audio_producer) return;

			auto speaker = audio_producer->get_speaker();
			if(speaker) {
				speaker->set_input_rate_multiplier(float(multiplier));
			}
		}

		/*!
			@returns The current speed multiplier.
		*/
		virtual double get_speed_multiplier() const {
			return speed_multiplier_;
		}

		/// @returns The confidence that this machine is running content it understands.
		virtual float get_confidence() { return 0.5f; }
		virtual std::string debug_type() { return ""; }

		/// Ensures all locally-buffered output is posted onward.
		virtual void flush_output() {}

	protected:
		/// Runs the machine for @c cycles.
		virtual void run_for(const Cycles cycles) = 0;

		/// Sets this machine's clock rate.
		void set_clock_rate(double clock_rate) {
			clock_rate_ = clock_rate;
		}

		/// Gets this machine's clock rate.
		double get_clock_rate() const {
			return clock_rate_;
		}

	private:
		// Give the ScanProducer access to this machine's clock rate.
		friend class ScanProducer;

		double clock_rate_ = 1.0;
		double clock_conversion_error_ = 0.0;
		double speed_multiplier_ = 1.0;
};

}

#endif /* TimedMachine_h */
