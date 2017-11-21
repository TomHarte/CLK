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

		ConfigurationTarget::Machine *configuration_target() override {
			return get<ConfigurationTarget::Machine>();
		}

		CRTMachine::Machine *crt_machine() override {
			return get<CRTMachine::Machine>();
		}

		JoystickMachine::Machine *joystick_machine() override {
			return get<JoystickMachine::Machine>();
		}

		KeyboardMachine::Machine *keyboard_machine() override {
			return get<KeyboardMachine::Machine>();
		}

		Configurable::Device *configurable_device() override {
			return get<Configurable::Device>();
		}

	private:
		template <typename Class> Class *get() {
			return dynamic_cast<Class *>(machine_.get());
		}
		std::unique_ptr<T> machine_;
};

}

::Machine::DynamicMachine *::Machine::MachineForTarget(const StaticAnalyser::Target &target) {
	switch(target.machine) {
		case StaticAnalyser::Target::AmstradCPC:	return new TypedDynamicMachine<AmstradCPC::Machine>(AmstradCPC::Machine::AmstradCPC());
		case StaticAnalyser::Target::Atari2600:		return new TypedDynamicMachine<Atari2600::Machine>(Atari2600::Machine::Atari2600());
		case StaticAnalyser::Target::Electron:		return new TypedDynamicMachine<Electron::Machine>(Electron::Machine::Electron());
		case StaticAnalyser::Target::Oric:			return new TypedDynamicMachine<Oric::Machine>(Oric::Machine::Oric());
		case StaticAnalyser::Target::Vic20:			return new TypedDynamicMachine<Commodore::Vic20::Machine>(Commodore::Vic20::Machine::Vic20());
		case StaticAnalyser::Target::ZX8081:		return new TypedDynamicMachine<ZX8081::Machine>(ZX8081::Machine::ZX8081(target));

		default:	return nullptr;
	}
}

std::string Machine::ShortNameForTargetMachine(const StaticAnalyser::Target::Machine machine) {
	switch(machine) {
		case StaticAnalyser::Target::AmstradCPC:	return "AmstradCPC";
		case StaticAnalyser::Target::Atari2600:		return "Atari2600";
		case StaticAnalyser::Target::Electron:		return "Electron";
		case StaticAnalyser::Target::Oric:			return "Oric";
		case StaticAnalyser::Target::Vic20:			return "Vic20";
		case StaticAnalyser::Target::ZX8081:		return "ZX8081";

		default:	return "";
	}
}

std::string Machine::LongNameForTargetMachine(StaticAnalyser::Target::Machine machine) {
	switch(machine) {
		case StaticAnalyser::Target::AmstradCPC:	return "Amstrad CPC";
		case StaticAnalyser::Target::Atari2600:		return "Atari 2600";
		case StaticAnalyser::Target::Electron:		return "Acorn Electron";
		case StaticAnalyser::Target::Oric:			return "Oric";
		case StaticAnalyser::Target::Vic20:			return "Vic 20";
		case StaticAnalyser::Target::ZX8081:		return "ZX80/81";

		default:	return "";
	}
}

std::map<std::string, std::vector<std::unique_ptr<Configurable::Option>>> Machine::AllOptionsByMachineName() {
	std::map<std::string, std::vector<std::unique_ptr<Configurable::Option>>> options;

	options.emplace(std::make_pair(LongNameForTargetMachine(StaticAnalyser::Target::Electron), Electron::get_options()));
	options.emplace(std::make_pair(LongNameForTargetMachine(StaticAnalyser::Target::Oric), Oric::get_options()));
	options.emplace(std::make_pair(LongNameForTargetMachine(StaticAnalyser::Target::Vic20), Commodore::Vic20::get_options()));
	options.emplace(std::make_pair(LongNameForTargetMachine(StaticAnalyser::Target::ZX8081), ZX8081::get_options()));

	return options;
}
