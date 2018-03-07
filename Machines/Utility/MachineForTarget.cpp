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
#include "../ColecoVision/ColecoVision.hpp"
#include "../Commodore/Vic-20/Vic20.hpp"
#include "../Electron/Electron.hpp"
#include "../MSX/MSX.hpp"
#include "../Oric/Oric.hpp"
#include "../ZX8081/ZX8081.hpp"

#include "../../Analyser/Dynamic/MultiMachine/MultiMachine.hpp"
#include "TypedDynamicMachine.hpp"

namespace {

::Machine::DynamicMachine *MachineForTarget(const Analyser::Static::Target &target, const ROMMachine::ROMFetcher &rom_fetcher, Machine::Error &error) {
	error = Machine::Error::None;
	::Machine::DynamicMachine *machine = nullptr;
	switch(target.machine) {
		case Analyser::Machine::AmstradCPC:		machine = new Machine::TypedDynamicMachine<AmstradCPC::Machine>(AmstradCPC::Machine::AmstradCPC());				break;
		case Analyser::Machine::Atari2600:		machine = new Machine::TypedDynamicMachine<Atari2600::Machine>(Atari2600::Machine::Atari2600());				break;
		case Analyser::Machine::ColecoVision:	machine = new Machine::TypedDynamicMachine<Coleco::Vision::Machine>(Coleco::Vision::Machine::ColecoVision());	break;
		case Analyser::Machine::Electron:		machine = new Machine::TypedDynamicMachine<Electron::Machine>(Electron::Machine::Electron());					break;
		case Analyser::Machine::MSX:			machine = new Machine::TypedDynamicMachine<MSX::Machine>(MSX::Machine::MSX());									break;
		case Analyser::Machine::Oric:			machine = new Machine::TypedDynamicMachine<Oric::Machine>(Oric::Machine::Oric());								break;
		case Analyser::Machine::Vic20:			machine = new Machine::TypedDynamicMachine<Commodore::Vic20::Machine>(Commodore::Vic20::Machine::Vic20());		break;
		case Analyser::Machine::ZX8081:			machine = new Machine::TypedDynamicMachine<ZX8081::Machine>(ZX8081::Machine::ZX8081(target));					break;

		default:
			error = Machine::Error::UnknownMachine;
		return nullptr;
	}

	// TODO: this shouldn't depend on CRT machine's inclusion of ROM machine.
	CRTMachine::Machine *crt_machine = machine->crt_machine();
	if(crt_machine) {
		if(!machine->crt_machine()->set_rom_fetcher(rom_fetcher)) {
			delete machine;
			error = Machine::Error::MissingROM;
			return nullptr;
		}
	}

	ConfigurationTarget::Machine *configuration_target = machine->configuration_target();
	if(configuration_target) {
		machine->configuration_target()->configure_as_target(target);
	}

	return machine;
}

}

::Machine::DynamicMachine *::Machine::MachineForTargets(const std::vector<std::unique_ptr<Analyser::Static::Target>> &targets, const ROMMachine::ROMFetcher &rom_fetcher, Error &error) {
	// Zero targets implies no machine.
	if(targets.empty()) {
		error = Error::NoTargets;
		return nullptr;
	}

	// If there's more than one target, get all the machines and combine them into a multimachine.
	if(targets.size() > 1) {
		std::vector<std::unique_ptr<Machine::DynamicMachine>> machines;
		for(const auto &target: targets) {
			machines.emplace_back(MachineForTarget(*target, rom_fetcher, error));

			// Exit early if any errors have occurred.
			if(error != Error::None) {
				return nullptr;
			}
		}

		// If a multimachine would just instantly collapse the list to a single machine, do
		// so without the ongoing baggage of a multimachine.
		if(Analyser::Dynamic::MultiMachine::would_collapse(machines)) {
			return machines.front().release();
		} else {
			return new Analyser::Dynamic::MultiMachine(std::move(machines));
		}
	}

	// There's definitely exactly one target.
	return MachineForTarget(*targets.front(), rom_fetcher, error);
}

std::string Machine::ShortNameForTargetMachine(const Analyser::Machine machine) {
	switch(machine) {
		case Analyser::Machine::AmstradCPC:		return "AmstradCPC";
		case Analyser::Machine::Atari2600:		return "Atari2600";
		case Analyser::Machine::ColecoVision:	return "ColecoVision";
		case Analyser::Machine::Electron:		return "Electron";
		case Analyser::Machine::MSX:			return "MSX";
		case Analyser::Machine::Oric:			return "Oric";
		case Analyser::Machine::Vic20:			return "Vic20";
		case Analyser::Machine::ZX8081:			return "ZX8081";

		default:	return "";
	}
}

std::string Machine::LongNameForTargetMachine(Analyser::Machine machine) {
	switch(machine) {
		case Analyser::Machine::AmstradCPC:		return "Amstrad CPC";
		case Analyser::Machine::Atari2600:		return "Atari 2600";
		case Analyser::Machine::ColecoVision:	return "ColecoVision";
		case Analyser::Machine::Electron:		return "Acorn Electron";
		case Analyser::Machine::MSX:			return "MSX";
		case Analyser::Machine::Oric:			return "Oric";
		case Analyser::Machine::Vic20:			return "Vic 20";
		case Analyser::Machine::ZX8081:			return "ZX80/81";

		default:	return "";
	}
}

std::map<std::string, std::vector<std::unique_ptr<Configurable::Option>>> Machine::AllOptionsByMachineName() {
	std::map<std::string, std::vector<std::unique_ptr<Configurable::Option>>> options;

	options.emplace(std::make_pair(LongNameForTargetMachine(Analyser::Machine::Electron), Electron::get_options()));
	options.emplace(std::make_pair(LongNameForTargetMachine(Analyser::Machine::MSX), MSX::get_options()));
	options.emplace(std::make_pair(LongNameForTargetMachine(Analyser::Machine::Oric), Oric::get_options()));
	options.emplace(std::make_pair(LongNameForTargetMachine(Analyser::Machine::Vic20), Commodore::Vic20::get_options()));
	options.emplace(std::make_pair(LongNameForTargetMachine(Analyser::Machine::ZX8081), ZX8081::get_options()));

	return options;
}
