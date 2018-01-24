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
#include "../MSX/MSX.hpp"
#include "../Oric/Oric.hpp"
#include "../ZX8081/ZX8081.hpp"

#include "TypedDynamicMachine.hpp"

::Machine::DynamicMachine *::Machine::MachineForTargets(const std::vector<StaticAnalyser::Target> &targets) {
	// TODO: deal with target lists containing more than one machine.
	switch(targets.front().machine) {
		case StaticAnalyser::Target::AmstradCPC:	return new TypedDynamicMachine<AmstradCPC::Machine>(AmstradCPC::Machine::AmstradCPC());
		case StaticAnalyser::Target::Atari2600:		return new TypedDynamicMachine<Atari2600::Machine>(Atari2600::Machine::Atari2600());
		case StaticAnalyser::Target::Electron:		return new TypedDynamicMachine<Electron::Machine>(Electron::Machine::Electron());
		case StaticAnalyser::Target::MSX:			return new TypedDynamicMachine<MSX::Machine>(MSX::Machine::MSX());
		case StaticAnalyser::Target::Oric:			return new TypedDynamicMachine<Oric::Machine>(Oric::Machine::Oric());
		case StaticAnalyser::Target::Vic20:			return new TypedDynamicMachine<Commodore::Vic20::Machine>(Commodore::Vic20::Machine::Vic20());
		case StaticAnalyser::Target::ZX8081:		return new TypedDynamicMachine<ZX8081::Machine>(ZX8081::Machine::ZX8081(targets.front()));

		default:	return nullptr;
	}
}

std::string Machine::ShortNameForTargetMachine(const StaticAnalyser::Target::Machine machine) {
	switch(machine) {
		case StaticAnalyser::Target::AmstradCPC:	return "AmstradCPC";
		case StaticAnalyser::Target::Atari2600:		return "Atari2600";
		case StaticAnalyser::Target::Electron:		return "Electron";
		case StaticAnalyser::Target::MSX:			return "MSX";
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
		case StaticAnalyser::Target::MSX:			return "MSX";
		case StaticAnalyser::Target::Oric:			return "Oric";
		case StaticAnalyser::Target::Vic20:			return "Vic 20";
		case StaticAnalyser::Target::ZX8081:		return "ZX80/81";

		default:	return "";
	}
}

std::map<std::string, std::vector<std::unique_ptr<Configurable::Option>>> Machine::AllOptionsByMachineName() {
	std::map<std::string, std::vector<std::unique_ptr<Configurable::Option>>> options;

	options.emplace(std::make_pair(LongNameForTargetMachine(StaticAnalyser::Target::Electron), Electron::get_options()));
	options.emplace(std::make_pair(LongNameForTargetMachine(StaticAnalyser::Target::MSX), MSX::get_options()));
	options.emplace(std::make_pair(LongNameForTargetMachine(StaticAnalyser::Target::Oric), Oric::get_options()));
	options.emplace(std::make_pair(LongNameForTargetMachine(StaticAnalyser::Target::Vic20), Commodore::Vic20::get_options()));
	options.emplace(std::make_pair(LongNameForTargetMachine(StaticAnalyser::Target::ZX8081), ZX8081::get_options()));

	return options;
}
