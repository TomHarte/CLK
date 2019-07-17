//
//  DynamicMachine.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/01/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#ifndef DynamicMachine_h
#define DynamicMachine_h

#include "../Configurable/Configurable.hpp"
#include "../Activity/Source.hpp"

#include "CRTMachine.hpp"
#include "JoystickMachine.hpp"
#include "KeyboardMachine.hpp"
#include "MediaTarget.hpp"
#include "MouseMachine.hpp"

#include "Utility/Typer.hpp"

namespace Machine {

/*!
	Provides the structure for owning a machine and dynamically casting it as desired without knowledge of
	the machine's parent class or, therefore, the need to establish a common one.
*/
struct DynamicMachine {
	virtual ~DynamicMachine() {}

	virtual Activity::Source *activity_source() = 0;
	virtual Configurable::Device *configurable_device() = 0;
	virtual CRTMachine::Machine *crt_machine() = 0;
	virtual JoystickMachine::Machine *joystick_machine() = 0;
	virtual KeyboardMachine::Machine *keyboard_machine() = 0;
	virtual MouseMachine::Machine *mouse_machine() = 0;
	virtual MediaTarget::Machine *media_target() = 0;

	/*!
		Provides a raw pointer to the underlying machine if and only if this dynamic machine really is
		only a single machine.

		Very unsafe. Very temporary.

		TODO: eliminate in favour of introspection for machine-specific inputs. This is here temporarily
		only to permit continuity of certain features in the Mac port that have not yet made their way
		to the SDL/console port.
	*/
	virtual void *raw_pointer() = 0;
};

}

#endif /* DynamicMachine_h */
