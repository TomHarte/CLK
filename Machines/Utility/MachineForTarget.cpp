//
//  MachineForTarget.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/11/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "MachineForTarget.hpp"

#include "../AmstradCPC/AmstradCPC.hpp"
#include "../Apple/AppleII/AppleII.hpp"
#include "../Apple/Macintosh/Macintosh.hpp"
#include "../Atari/2600/Atari2600.hpp"
#include "../Atari/ST/AtariST.hpp"
#include "../ColecoVision/ColecoVision.hpp"
#include "../Commodore/Vic-20/Vic20.hpp"
#include "../Electron/Electron.hpp"
#include "../MasterSystem/MasterSystem.hpp"
#include "../MSX/MSX.hpp"
#include "../Oric/Oric.hpp"
#include "../ZX8081/ZX8081.hpp"

#include "../../Analyser/Dynamic/MultiMachine/MultiMachine.hpp"
#include "TypedDynamicMachine.hpp"

namespace {

::Machine::DynamicMachine *MachineForTarget(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher, Machine::Error &error) {
	error = Machine::Error::None;

	Machine::DynamicMachine *machine = nullptr;
	try {
#define BindD(name, m)	case Analyser::Machine::m: machine = new Machine::TypedDynamicMachine<name::Machine>(name::Machine::m(target, rom_fetcher));	break;
#define Bind(m)	BindD(m, m)
		switch(target->machine) {
			Bind(AmstradCPC)
			BindD(Apple::II, AppleII)
			BindD(Apple::Macintosh, Macintosh)
			Bind(Atari2600)
			BindD(Atari::ST, AtariST)
			BindD(Coleco::Vision, ColecoVision)
			BindD(Commodore::Vic20, Vic20)
			Bind(Electron)
			Bind(MSX)
			Bind(Oric)
			BindD(Sega::MasterSystem, MasterSystem)
			Bind(ZX8081)

			default:
				error = Machine::Error::UnknownMachine;
			return nullptr;
		}
#undef Bind
	} catch(ROMMachine::Error construction_error) {
		switch(construction_error) {
			case ROMMachine::Error::MissingROMs:
				error = Machine::Error::MissingROM;
			break;
			default:
				error = Machine::Error::UnknownError;
			break;
		}
	}

	return machine;
}

}

::Machine::DynamicMachine *::Machine::MachineForTargets(const Analyser::Static::TargetList &targets, const ROMMachine::ROMFetcher &rom_fetcher, Error &error) {
	// Zero targets implies no machine.
	if(targets.empty()) {
		error = Error::NoTargets;
		return nullptr;
	}

	// If there's more than one target, get all the machines and combine them into a multimachine.
	if(targets.size() > 1) {
		std::vector<std::unique_ptr<Machine::DynamicMachine>> machines;
		for(const auto &target: targets) {
			machines.emplace_back(MachineForTarget(target.get(), rom_fetcher, error));

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
	return MachineForTarget(targets.front().get(), rom_fetcher, error);
}

std::string Machine::ShortNameForTargetMachine(const Analyser::Machine machine) {
	switch(machine) {
		case Analyser::Machine::AmstradCPC:		return "AmstradCPC";
		case Analyser::Machine::AppleII:		return "AppleII";
		case Analyser::Machine::Atari2600:		return "Atari2600";
		case Analyser::Machine::AtariST:		return "AtariST";
		case Analyser::Machine::ColecoVision:	return "ColecoVision";
		case Analyser::Machine::Electron:		return "Electron";
		case Analyser::Machine::Macintosh:		return "Macintosh";
		case Analyser::Machine::MasterSystem:	return "MasterSystem";
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
		case Analyser::Machine::AppleII:		return "Apple II";
		case Analyser::Machine::Atari2600:		return "Atari 2600";
		case Analyser::Machine::AtariST:		return "Atari ST";
		case Analyser::Machine::ColecoVision:	return "ColecoVision";
		case Analyser::Machine::Electron:		return "Acorn Electron";
		case Analyser::Machine::Macintosh:		return "Apple Macintosh";
		case Analyser::Machine::MasterSystem:	return "Sega Master System";
		case Analyser::Machine::MSX:			return "MSX";
		case Analyser::Machine::Oric:			return "Oric";
		case Analyser::Machine::Vic20:			return "Vic 20";
		case Analyser::Machine::ZX8081:			return "ZX80/81";

		default:	return "";
	}
}

std::map<std::string, std::vector<std::unique_ptr<Configurable::Option>>> Machine::AllOptionsByMachineName() {
	std::map<std::string, std::vector<std::unique_ptr<Configurable::Option>>> options;

	options.emplace(std::make_pair(LongNameForTargetMachine(Analyser::Machine::AmstradCPC), AmstradCPC::get_options()));
	options.emplace(std::make_pair(LongNameForTargetMachine(Analyser::Machine::AppleII), Apple::II::get_options()));
	options.emplace(std::make_pair(LongNameForTargetMachine(Analyser::Machine::ColecoVision), Coleco::Vision::get_options()));
	options.emplace(std::make_pair(LongNameForTargetMachine(Analyser::Machine::Electron), Electron::get_options()));
	options.emplace(std::make_pair(LongNameForTargetMachine(Analyser::Machine::Macintosh), Apple::Macintosh::get_options()));
	options.emplace(std::make_pair(LongNameForTargetMachine(Analyser::Machine::MasterSystem), Sega::MasterSystem::get_options()));
	options.emplace(std::make_pair(LongNameForTargetMachine(Analyser::Machine::MSX), MSX::get_options()));
	options.emplace(std::make_pair(LongNameForTargetMachine(Analyser::Machine::Oric), Oric::get_options()));
	options.emplace(std::make_pair(LongNameForTargetMachine(Analyser::Machine::Vic20), Commodore::Vic20::get_options()));
	options.emplace(std::make_pair(LongNameForTargetMachine(Analyser::Machine::ZX8081), ZX8081::get_options()));

	return options;
}
