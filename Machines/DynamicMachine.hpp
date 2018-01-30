//
//  DynamicMachine.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/01/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#ifndef DynamicMachine_h
#define DynamicMachine_h

#include "../Configurable/Configurable.hpp"
#include "ConfigurationTarget.hpp"
#include "CRTMachine.hpp"
#include "JoystickMachine.hpp"
#include "KeyboardMachine.hpp"
#include "Utility/Typer.hpp"

namespace Machine {

/*!
	Provides the structure for owning a machine and dynamically casting it as desired without knowledge of
	the machine's parent class or, therefore, the need to establish a common one.
*/
struct DynamicMachine {
	virtual ~DynamicMachine() {}
	virtual ConfigurationTarget::Machine *configuration_target() = 0;
	virtual CRTMachine::Machine *crt_machine() = 0;
	virtual JoystickMachine::Machine *joystick_machine() = 0;
	virtual KeyboardMachine::Machine *keyboard_machine() = 0;
	virtual Configurable::Device *configurable_device() = 0;
	virtual Utility::TypeRecipient *type_recipient() = 0;
};

}

#endif /* DynamicMachine_h */
