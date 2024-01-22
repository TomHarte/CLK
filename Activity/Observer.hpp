//
//  ActivityObserver.h
//  Clock Signal
//
//  Created by Thomas Harte on 07/05/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#pragma once

#include <cstdint>
#include <string>

namespace Activity {

/*!
	Provides a purely virtual base class for anybody that wants to receive notifications of
	'activity': any feedback from an emulated system which a user could perceive other than
	through the machine's native audio and video outputs.

	So: status LEDs, drive activity, etc. A receiver may choose to make appropriate noises
	and/or to show or unshow status indicators.
*/
class Observer {
	public:
		virtual ~Observer() {}

		/// Provides hints as to the sort of information presented on an LED.
		enum LEDPresentation: uint8_t {
			/// This LED informs the user of some sort of persistent state, e.g. scroll lock.
			/// If this flag is absent then the LED describes an ephemeral state, such as media access.
			Persistent = (1 << 0),
		};

		/// Announces to the receiver that there is an LED of name @c name.
		virtual void register_led([[maybe_unused]] const std::string &name, [[maybe_unused]] uint8_t presentation = 0) {}

		/// Announces to the receiver that there is a drive of name @c name.
		///
		/// If a drive has the same name as an LED, that LED goes with this drive.
		virtual void register_drive([[maybe_unused]] const std::string &name) {}

		/// Informs the receiver of the new state of the LED with name @c name.
		virtual void set_led_status([[maybe_unused]] const std::string &name, [[maybe_unused]] bool lit) {}

		enum class DriveEvent {
			StepNormal,
			StepBelowZero,
			StepBeyondMaximum
		};

		/// Informs the receiver that the named event just occurred for the drive with name @c name.
		virtual void announce_drive_event([[maybe_unused]] const std::string &name, [[maybe_unused]] DriveEvent event) {}

		/// Informs the receiver of the motor-on status of the drive with name @c name.
		virtual void set_drive_motor_status([[maybe_unused]] const std::string &name, [[maybe_unused]] bool is_on) {}
};

}
