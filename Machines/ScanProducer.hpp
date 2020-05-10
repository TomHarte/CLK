//
//  ScanProducer.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 31/03/2020.
//  Copyright 2020 Thomas Harte. All rights reserved.
//

#ifndef ScanProducer_hpp
#define ScanProducer_hpp

#include "../Outputs/ScanTarget.hpp"
#include "../Configurable/StandardOptions.hpp"

#include "TimedMachine.hpp"

namespace MachineTypes {

/*!
	A ScanProducer::Producer is any machine that produces video output of the form accepted
	by a ScanTarget.
*/
class ScanProducer {
	public:
		/*!
			Causes the machine to set up its display and, if it has one, speaker.

			The @c scan_target will receive all video output; the caller guarantees
			that it is non-null.
		*/
		virtual void set_scan_target(Outputs::Display::ScanTarget *scan_target) = 0;

		/*!
			@returns The current scan status.
		*/
		virtual Outputs::Display::ScanStatus get_scan_status() const {
			// There's an implicit assumption here that anything which produces scans
			// is also a timed machine. And, also, that this function will be called infrequently.
			const TimedMachine *timed_machine = dynamic_cast<const TimedMachine *>(this);
			return get_scaled_scan_status() / float(timed_machine->get_clock_rate());
		}

	protected:
		virtual Outputs::Display::ScanStatus get_scaled_scan_status() const {
			// This deliberately sets up an infinite loop if the user hasn't
			// overridden at least one of this or get_scan_status.
			//
			// Most likely you want to override this, and let the base class
			// throw in a divide-by-clock-rate at the end for you.
			return get_scan_status();
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
			Maps back from Outputs::Display::VideoSignal  to Configurable::Display,
			calling @c get_display_type for the input.
		*/
		Configurable::Display get_video_signal_configurable() {
			switch(get_display_type()) {
				default:
				case Outputs::Display::DisplayType::RGB:					return Configurable::Display::RGB;
				case Outputs::Display::DisplayType::SVideo: 				return Configurable::Display::SVideo;
				case Outputs::Display::DisplayType::CompositeColour: 		return Configurable::Display::CompositeColour;
				case Outputs::Display::DisplayType::CompositeMonochrome:	return Configurable::Display::CompositeMonochrome;
			}
		}

		/*!
			Sets the display type.
		*/
		virtual void set_display_type(Outputs::Display::DisplayType display_type) {}

		/*!
			Gets the display type.
		*/
		virtual Outputs::Display::DisplayType get_display_type() { return Outputs::Display::DisplayType::RGB; }
};

}

#endif /* ScanProducer_hpp */
