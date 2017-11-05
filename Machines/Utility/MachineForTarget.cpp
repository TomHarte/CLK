//
//  MachineForTarget.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/11/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "MachineForTarget.hpp"

#include "../AmstradCPC/AmstradCPC.hpp"
#include "../Atari2600/Atari2600.hpp"
#include "../Commodore/Vic-20/Vic20.hpp"
#include "../Electron/Electron.hpp"
#include "../Oric/Oric.hpp"
#include "../ZX8081/ZX8081.hpp"

namespace {

template<typename T> class TypedDynamicMachine: public ::Machine::DynamicMachine {
	public:
		TypedDynamicMachine(T *machine) : machine_(machine) {}

		ConfigurationTarget::Machine *configuration_target() {
			return dynamic_cast<ConfigurationTarget::Machine *>(machine_.get());
		}

		CRTMachine::Machine *crt_machine() {
			return dynamic_cast<CRTMachine::Machine *>(machine_.get());
		}

		JoystickMachine::Machine *joystick_machine() {
			return dynamic_cast<JoystickMachine::Machine *>(machine_.get());
		}

		KeyboardMachine::Machine *keyboard_machine() {
			return dynamic_cast<KeyboardMachine::Machine *>(machine_.get());
		}

	private:
		std::unique_ptr<T> machine_;
};

}

::Machine::DynamicMachine *::Machine::MachineForTarget(const StaticAnalyser::Target &target) {
	switch(target.machine) {
		case StaticAnalyser::Target::AmstradCPC:	return new TypedDynamicMachine<AmstradCPC::Machine>(AmstradCPC::Machine::AmstradCPC());
//		case StaticAnalyser::Target::Atari2600:		return new TypedDynamicMachine(Atari2600::Machine::Atari2600());
//		case StaticAnalyser::Target::Electron:		return new TypedDynamicMachine(Electron::Machine::Electron());
//		case StaticAnalyser::Target::Oric:			return new TypedDynamicMachine(Oric::Machine::Oric());
//		case StaticAnalyser::Target::Vic20:			return new TypedDynamicMachine(Commodore::Vic20::Machine::Vic20());
//		case StaticAnalyser::Target::ZX8081:		return new TypedDynamicMachine(ZX8081::Machine::ZX8081(target));
	default: return nullptr;
	}
}
