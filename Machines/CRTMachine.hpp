//
//  CRTMachine.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 31/05/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#ifndef CRTMachine_hpp
#define CRTMachine_hpp

#include "../Outputs/ScanTarget.hpp"
#include "../Outputs/Speaker/Speaker.hpp"
#include "../ClockReceiver/ClockReceiver.hpp"
#include "../ClockReceiver/TimeTypes.hpp"
#include "ROMMachine.hpp"

#include "../Configurable/StandardOptions.hpp"

#include <cmath>

// TODO: rename.
namespace CRTMachine {

/*!
	A CRTMachine::Machine is a mostly-abstract base class for machines that connect to a CRT,
	that optionally provide a speaker, and that nominate a clock rate and can announce to a delegate
	should that clock rate change.
*/
class Machine {
	public:
		/*!
			Causes the machine to set up its display and, if it has one, speaker.

			The @c scan_target will receive all video output; the caller guarantees
			that it is non-null.
		*/
		virtual void set_scan_target(Outputs::Display::ScanTarget *scan_target) = 0;

		/// @returns The speaker that receives this machine's output, or @c nullptr if this machine is mute.
		virtual Outputs::Speaker::Speaker *get_speaker() = 0;

		/// @returns The confidence that this machine is running content it understands.
		virtual float get_confidence() { return 0.5f; }
		virtual std::string debug_type() { return ""; }

		/// Runs the machine for @c duration seconds.
		void run_for(Time::Seconds duration) {
			const double cycles = (duration * clock_rate_) + clock_conversion_error_;
			clock_conversion_error_ = std::fmod(cycles, 1.0);
			run_for(Cycles(static_cast<int>(cycles)));
		}

		/// Runs for the machine for at least @c duration seconds, and then until @c condition is true.
		void run_until(Time::Seconds minimum_duration, std::function<bool()> condition) {
			run_for(minimum_duration);
			while(!condition()) {
				run_for(0.002);
			}
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
			Maps from Configurable::Display to Outputs::Display::VideoSignal and calls
			@c set_display_type with the result.
		*/
		void set_video_signal_configurable(Configurable::Display type) {
			Outputs::Display::DisplayType display_type;
			switch(type) {
				default:
				case Configurable::Display::RGB:
					display_type = Outputs::Display::DisplayType::RGB;
				break;
				case Configurable::Display::SVideo:
					display_type = Outputs::Display::DisplayType::SVideo;
				break;
				case Configurable::Display::CompositeColour:
					display_type = Outputs::Display::DisplayType::CompositeColour;
				break;
				case Configurable::Display::CompositeMonochrome:
					display_type = Outputs::Display::DisplayType::CompositeMonochrome;
				break;
			}
			set_display_type(display_type);
		}

		/*!
			Forwards the video signal to the target returned by get_crt().
		*/
		virtual void set_display_type(Outputs::Display::DisplayType display_type) {}

	private:
		double clock_rate_ = 1.0;
		double clock_conversion_error_ = 0.0;
};

}

#endif /* CRTMachine_hpp */
