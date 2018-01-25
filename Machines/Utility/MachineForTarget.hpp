//
//  MachineForTarget.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/11/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef MachineForTarget_hpp
#define MachineForTarget_hpp

#include "../../Analyser/Static/StaticAnalyser.hpp"

#include "../../Configurable/Configurable.hpp"
#include "../ConfigurationTarget.hpp"
#include "../CRTMachine.hpp"
#include "../JoystickMachine.hpp"
#include "../KeyboardMachine.hpp"
#include "Typer.hpp"

#include <map>
#include <string>
#include <vector>

namespace Machine {

/*!
	Provides the structure for owning a machine and dynamically casting it as desired without knowledge of
	the machine's parent class or, therefore, the need to establish a common one.
*/
struct DynamicMachine {
	virtual ConfigurationTarget::Machine *configuration_target() = 0;
	virtual CRTMachine::Machine *crt_machine() = 0;
	virtual JoystickMachine::Machine *joystick_machine() = 0;
	virtual KeyboardMachine::Machine *keyboard_machine() = 0;
	virtual Configurable::Device *configurable_device() = 0;
	virtual Utility::TypeRecipient *type_recipient() = 0;
};

/*!
	Allocates an instance of DynamicMachine holding a machine that can
	receive the supplied static analyser result. The machine has been allocated
	on the heap. It is the caller's responsibility to delete the class when finished.
*/
DynamicMachine *MachineForTargets(const std::vector<Analyser::Static::Target> &target);

/*!
	Returns a short string name for the machine identified by the target,
	which is guaranteed not to have any spaces or other potentially
	filesystem-bothering contents.
*/
std::string ShortNameForTargetMachine(const Analyser::Machine target);

/*!
	Returns a long string name for the machine identified by the target,
	usable for presentation to a human.
*/
std::string LongNameForTargetMachine(const Analyser::Machine target);

/*!
	Returns a map from machine name to the list of options that machine
	exposes, for all machines.
*/
std::map<std::string, std::vector<std::unique_ptr<Configurable::Option>>> AllOptionsByMachineName();

}

#endif /* MachineForTarget_hpp */
