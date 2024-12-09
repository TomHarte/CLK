//
//  MachineForTarget.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/11/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "MachineForTarget.hpp"

#include <algorithm>

// Sources for runtime options and machines.
#include "../Acorn/Archimedes/Archimedes.hpp"
#include "../Acorn/Electron/Electron.hpp"
#include "../Amiga/Amiga.hpp"
#include "../AmstradCPC/AmstradCPC.hpp"
#include "../Apple/AppleII/AppleII.hpp"
#include "../Apple/AppleIIgs/AppleIIgs.hpp"
#include "../Apple/Macintosh/Macintosh.hpp"
#include "../Atari/2600/Atari2600.hpp"
#include "../Atari/ST/AtariST.hpp"
#include "../ColecoVision/ColecoVision.hpp"
#include "../Commodore/Plus4/Plus4.hpp"
#include "../Commodore/Vic-20/Vic20.hpp"
#include "../Enterprise/Enterprise.hpp"
#include "../MasterSystem/MasterSystem.hpp"
#include "../MSX/MSX.hpp"
#include "../Oric/Oric.hpp"
#include "../PCCompatible/PCCompatible.hpp"
#include "../Sinclair/ZX8081/ZX8081.hpp"
#include "../Sinclair/ZXSpectrum/ZXSpectrum.hpp"

// Sources for construction options.
#include "../../Analyser/Static/Acorn/Target.hpp"
#include "../../Analyser/Static/Amiga/Target.hpp"
#include "../../Analyser/Static/AmstradCPC/Target.hpp"
#include "../../Analyser/Static/AppleII/Target.hpp"
#include "../../Analyser/Static/AppleIIgs/Target.hpp"
#include "../../Analyser/Static/Atari2600/Target.hpp"
#include "../../Analyser/Static/AtariST/Target.hpp"
#include "../../Analyser/Static/Commodore/Target.hpp"
#include "../../Analyser/Static/Enterprise/Target.hpp"
#include "../../Analyser/Static/Macintosh/Target.hpp"
#include "../../Analyser/Static/MSX/Target.hpp"
#include "../../Analyser/Static/Oric/Target.hpp"
#include "../../Analyser/Static/PCCompatible/Target.hpp"
#include "../../Analyser/Static/Sega/Target.hpp"
#include "../../Analyser/Static/ZX8081/Target.hpp"
#include "../../Analyser/Static/ZXSpectrum/Target.hpp"

#include "../../Analyser/Dynamic/MultiMachine/MultiMachine.hpp"
#include "TypedDynamicMachine.hpp"

std::unique_ptr<Machine::DynamicMachine> Machine::MachineForTarget(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher, Machine::Error &error) {
	error = Machine::Error::None;

	std::unique_ptr<Machine::DynamicMachine> machine;
	try {
#define BindD(name, m)	case Analyser::Machine::m: machine = std::make_unique<Machine::TypedDynamicMachine<::name::Machine>>(name::Machine::m(target, rom_fetcher));	break;
#define Bind(m)	BindD(m, m)
		switch(target->machine) {
			Bind(Amiga)
			Bind(AmstradCPC)
			Bind(Archimedes)
			BindD(Apple::II, AppleII)
			BindD(Apple::IIgs, AppleIIgs)
			BindD(Apple::Macintosh, Macintosh)
			Bind(Atari2600)
			BindD(Atari::ST, AtariST)
			BindD(Coleco::Vision, ColecoVision)
			BindD(Commodore::Plus4, Plus4)
			BindD(Commodore::Vic20, Vic20)
			Bind(Electron)
			Bind(Enterprise)
			Bind(MSX)
			Bind(Oric)
			Bind(PCCompatible)
			BindD(Sega::MasterSystem, MasterSystem)
			BindD(Sinclair::ZX8081, ZX8081)
			BindD(Sinclair::ZXSpectrum, ZXSpectrum)

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

std::unique_ptr<Machine::DynamicMachine> Machine::MachineForTargets(const Analyser::Static::TargetList &targets, const ROMMachine::ROMFetcher &rom_fetcher, Error &error) {
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
			return std::move(machines.front());
		} else {
			return std::make_unique<Analyser::Dynamic::MultiMachine>(std::move(machines));
		}
	}

	// There's definitely exactly one target.
	return MachineForTarget(targets.front().get(), rom_fetcher, error);
}

std::string Machine::ShortNameForTargetMachine(const Analyser::Machine machine) {
	switch(machine) {
		case Analyser::Machine::Amiga:			return "Amiga";
		case Analyser::Machine::AmstradCPC:		return "AmstradCPC";
		case Analyser::Machine::AppleII:		return "AppleII";
		case Analyser::Machine::AppleIIgs:		return "AppleIIgs";
		case Analyser::Machine::Archimedes:		return "Archimedes";
		case Analyser::Machine::Atari2600:		return "Atari2600";
		case Analyser::Machine::AtariST:		return "AtariST";
		case Analyser::Machine::ColecoVision:	return "ColecoVision";
		case Analyser::Machine::Electron:		return "Electron";
		case Analyser::Machine::Enterprise:		return "Enterprise";
		case Analyser::Machine::Macintosh:		return "Macintosh";
		case Analyser::Machine::MasterSystem:	return "MasterSystem";
		case Analyser::Machine::MSX:			return "MSX";
		case Analyser::Machine::Oric:			return "Oric";
		case Analyser::Machine::Plus4:			return "Plus4";
		case Analyser::Machine::PCCompatible:	return "PCCompatible";
		case Analyser::Machine::Vic20:			return "Vic20";
		case Analyser::Machine::ZX8081:			return "ZX8081";
		case Analyser::Machine::ZXSpectrum:		return "ZXSpectrum";

		default:	return "";
	}
}

std::string Machine::LongNameForTargetMachine(Analyser::Machine machine) {
	switch(machine) {
		case Analyser::Machine::Amiga:			return "Amiga";
		case Analyser::Machine::AmstradCPC:		return "Amstrad CPC";
		case Analyser::Machine::AppleII:		return "Apple II";
		case Analyser::Machine::AppleIIgs:		return "Apple IIgs";
		case Analyser::Machine::Archimedes:		return "Acorn Archimedes";
		case Analyser::Machine::Atari2600:		return "Atari 2600";
		case Analyser::Machine::AtariST:		return "Atari ST";
		case Analyser::Machine::ColecoVision:	return "ColecoVision";
		case Analyser::Machine::Electron:		return "Acorn Electron";
		case Analyser::Machine::Enterprise:		return "Enterprise";
		case Analyser::Machine::Macintosh:		return "Apple Macintosh";
		case Analyser::Machine::MasterSystem:	return "Sega Master System";
		case Analyser::Machine::MSX:			return "MSX";
		case Analyser::Machine::Oric:			return "Oric";
		case Analyser::Machine::Plus4:			return "Commodore C16+4";
		case Analyser::Machine::PCCompatible:	return "PC Compatible";
		case Analyser::Machine::Vic20:			return "Vic 20";
		case Analyser::Machine::ZX8081:			return "ZX80/81";
		case Analyser::Machine::ZXSpectrum:		return "ZX Spectrum";

		default:	return "";
	}
}

std::vector<std::string> Machine::AllMachines(Type type, bool long_names) {
	std::vector<std::string> result;

#define AddName(x) result.push_back(long_names ? LongNameForTargetMachine(Analyser::Machine::x) : ShortNameForTargetMachine(Analyser::Machine::x))

	if(type == Type::Any || type == Type::RequiresMedia) {
		AddName(Atari2600);
		AddName(ColecoVision);
		AddName(MasterSystem);
	}

	if(type == Type::Any || type == Type::DoesntRequireMedia) {
		AddName(Amiga);
		AddName(AmstradCPC);
		AddName(AppleII);
		AddName(AppleIIgs);
		AddName(Archimedes);
		AddName(AtariST);
		AddName(Electron);
		AddName(Enterprise);
		AddName(Macintosh);
		AddName(MSX);
		AddName(Oric);
		AddName(Plus4);
		AddName(PCCompatible);
		AddName(Vic20);
		AddName(ZX8081);
		AddName(ZXSpectrum);
	}

#undef AddName

	return result;
}

std::map<std::string, std::unique_ptr<Reflection::Struct>> Machine::AllOptionsByMachineName() {
	std::map<std::string, std::unique_ptr<Reflection::Struct>> options;

#define Emplace(machine, class)	\
	options.emplace(LongNameForTargetMachine(Analyser::Machine::machine), std::make_unique<class::Options>(Configurable::OptionsType::UserFriendly))

	Emplace(AmstradCPC, AmstradCPC::Machine);
	Emplace(AppleII, Apple::II::Machine);
	Emplace(Archimedes, Archimedes::Machine);
	Emplace(AtariST, Atari::ST::Machine);
	Emplace(ColecoVision, Coleco::Vision::Machine);
	Emplace(Electron, Electron::Machine);
	Emplace(Enterprise, Enterprise::Machine);
	Emplace(Macintosh, Apple::Macintosh::Machine);
	Emplace(MasterSystem, Sega::MasterSystem::Machine);
	Emplace(MSX, MSX::Machine);
	Emplace(Oric, Oric::Machine);
//	Emplace(Plus4, Commodore::Plus4::Machine);		// There are no options yet.
	Emplace(PCCompatible, PCCompatible::Machine);
	Emplace(Vic20, Commodore::Vic20::Machine);
	Emplace(ZX8081, Sinclair::ZX8081::Machine);
	Emplace(ZXSpectrum, Sinclair::ZXSpectrum::Machine);

#undef Emplace

	return options;
}

std::map<std::string, std::unique_ptr<Analyser::Static::Target>> Machine::TargetsByMachineName(bool meaningful_without_media_only) {
	std::map<std::string, std::unique_ptr<Analyser::Static::Target>> options;

#define AddMapped(Name, TargetNamespace)	\
	options.emplace(LongNameForTargetMachine(Analyser::Machine::Name), std::make_unique<Analyser::Static::TargetNamespace::Target>());
#define Add(Name)	AddMapped(Name, Name)

	Add(Amiga);
	Add(AmstradCPC);
	Add(AppleII);
	Add(AppleIIgs);
	options.emplace(LongNameForTargetMachine(Analyser::Machine::Archimedes), std::make_unique<Analyser::Static::Acorn::ArchimedesTarget>());
	Add(AtariST);
	options.emplace(LongNameForTargetMachine(Analyser::Machine::Electron), std::make_unique<Analyser::Static::Acorn::ElectronTarget>());
	Add(Enterprise);
	Add(Macintosh);
	Add(MSX);
	Add(Oric);
	AddMapped(Plus4, Commodore);
	Add(PCCompatible);
	AddMapped(Vic20, Commodore);
	Add(ZX8081);
	Add(ZXSpectrum);

	if(!meaningful_without_media_only) {
		Add(Atari2600);
		options.emplace(LongNameForTargetMachine(Analyser::Machine::ColecoVision), std::make_unique<Analyser::Static::Target>(Analyser::Machine::ColecoVision));
		AddMapped(MasterSystem, Sega);
	}

#undef Add
#undef AddMapped

	return options;
}
