//
//  ActivityObserver.h
//  Clock Signal
//
//  Created by Thomas Harte on 07/05/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#ifndef ActivityObserver_h
#define ActivityObserver_h

#include <string>

namespace Activity {

/*!
	Provides a purely virtual base class for anybody that wants to receive notifications of
	'activity' — any feedback from an emulated system which a user could perceive other than
	through the machine's native audio and video outputs.

	So: status LEDs, drive activity, etc. A receiver may choose to make appropriate noises
	and/or to show or unshow status indicators.
*/
class Observer {
	public:
		/// Announces to the receiver that there is an LED of name @c name.
		virtual void register_led(const std::string &name) = 0;

		/// Announces to the receiver that there is a drive of name @c name.
		virtual void register_drive(const std::string &name) = 0;

		/// Informs the receiver of the new state of the LED with name @c name.
		virtual void set_led_status(const std::string &name, bool lit) = 0;

		enum class DriveEvent {
			StepNormal,
			StepBelowZero,
			StepBeyondMaximum
		};

		/// Informs the receiver that the named event just occurred for the drive with name @c name.
		virtual void announce_drive_event(const std::string &name, DriveEvent event) = 0;

		/// Informs the receiver of the motor-on status of the drive with name @c name.
		virtual void set_drive_motor_status(const std::string &name, bool is_on) = 0;

};

}

#endif /* ActivityObserver_h */
