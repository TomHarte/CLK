//
//  CRTMachine.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 31/05/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#ifndef CRTMachine_hpp
#define CRTMachine_hpp

#include "../Outputs/CRT/CRT.hpp"
#include "../Outputs/Speaker/Speaker.hpp"
#include "../ClockReceiver/ClockReceiver.hpp"
#include "../ClockReceiver/TimeTypes.hpp"
#include "ROMMachine.hpp"

#include "../Configurable/StandardOptions.hpp"

#include <cmath>

namespace CRTMachine {

/*!
	A CRTMachine::Machine is a mostly-abstract base class for machines that connect to a CRT,
	that optionally provide a speaker, and that nominate a clock rate and can announce to a delegate
	should that clock rate change.
*/
class Machine {
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

	protected:
		/// Runs the machine for @c cycles.
		virtual void run_for(const Cycles cycles) = 0;
		void set_clock_rate(double clock_rate) {
			clock_rate_ = clock_rate;
		}
		double get_clock_rate() {
			return clock_rate_;
		}

		/*!
			Maps from Configurable::Display to Outputs::CRT::VideoSignal and calls
			@c set_video_signal with the result.
		*/
		void set_video_signal_configurable(Configurable::Display type) {
			Outputs::CRT::VideoSignal signal;
			switch(type) {
				default:
				case Configurable::Display::RGB:
					signal = Outputs::CRT::VideoSignal::RGB;
				break;
				case Configurable::Display::SVideo:
					signal = Outputs::CRT::VideoSignal::SVideo;
				break;
				case Configurable::Display::Composite:
					signal = Outputs::CRT::VideoSignal::Composite;
				break;
			}
			set_video_signal(signal);
		}

		/*!
			Forwards the video signal to the CRT returned by get_crt().
		*/
		virtual void set_video_signal(Outputs::CRT::VideoSignal video_signal) {
			get_crt()->set_video_signal(video_signal);
		}

	private:
		double clock_rate_ = 1.0;
		double clock_conversion_error_ = 0.0;
};

}

#endif /* CRTMachine_hpp */
