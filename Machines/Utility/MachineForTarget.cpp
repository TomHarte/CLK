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
#include "Machines/Acorn/Archimedes/Archimedes.hpp"
#include "Machines/Acorn/BBCMicro/BBCMicro.hpp"
#include "Machines/Acorn/Electron/Electron.hpp"
#include "Machines/Amiga/Amiga.hpp"
#include "Machines/AmstradCPC/AmstradCPC.hpp"
#include "Machines/Apple/AppleII/AppleII.hpp"
#include "Machines/Apple/AppleIIgs/AppleIIgs.hpp"
#include "Machines/Apple/Macintosh/Macintosh.hpp"
#include "Machines/Atari/2600/Atari2600.hpp"
#include "Machines/Atari/ST/AtariST.hpp"
#include "Machines/ColecoVision/ColecoVision.hpp"
#include "Machines/Commodore/Plus4/Plus4.hpp"
#include "Machines/Commodore/Vic-20/Vic20.hpp"
#include "Machines/Enterprise/Enterprise.hpp"
#include "Machines/MasterSystem/MasterSystem.hpp"
#include "Machines/MSX/MSX.hpp"
#include "Machines/Oric/Oric.hpp"
#include "Machines/PCCompatible/PCCompatible.hpp"
#include "Machines/Tandy/CoCo/CoCo.hpp"
#include "Machines/Thomson/MO/MO.hpp"
#include "Machines/Sinclair/ZX8081/ZX8081.hpp"
#include "Machines/Sinclair/ZXSpectrum/ZXSpectrum.hpp"

// Sources for construction options.
#include "Analyser/Static/Acorn/Target.hpp"
#include "Analyser/Static/Amiga/Target.hpp"
#include "Analyser/Static/AmstradCPC/Target.hpp"
#include "Analyser/Static/AppleII/Target.hpp"
#include "Analyser/Static/AppleIIgs/Target.hpp"
#include "Analyser/Static/Atari2600/Target.hpp"
#include "Analyser/Static/AtariST/Target.hpp"
#include "Analyser/Static/Commodore/Target.hpp"
#include "Analyser/Static/Enterprise/Target.hpp"
#include "Analyser/Static/Macintosh/Target.hpp"
#include "Analyser/Static/MSX/Target.hpp"
#include "Analyser/Static/Oric/Target.hpp"
#include "Analyser/Static/PCCompatible/Target.hpp"
#include "Analyser/Static/Sega/Target.hpp"
#include "Analyser/Static/Thomson/Target.hpp"
#include "Analyser/Static/ZX8081/Target.hpp"
#include "Analyser/Static/ZXSpectrum/Target.hpp"

#include "Analyser/Dynamic/MultiMachine/MultiMachine.hpp"
#include "TypedDynamicMachine.hpp"

namespace {
struct MachineGenerator {
	MachineGenerator(
		const Analyser::Static::Target *const target,
		const ROMMachine::ROMFetcher &rom_fetcher
	) : target(target), rom_fetcher(rom_fetcher) {}

	const Analyser::Static::Target *const target;
	const ROMMachine::ROMFetcher &rom_fetcher;

	std::unique_ptr<Machine::DynamicMachine> machine;

	template <typename MachineT>
	void posit(const Analyser::Machine name) {
		if(target->machine == name) {
			machine = std::make_unique<Machine::TypedDynamicMachine<MachineT>>(MachineT::create(target, rom_fetcher));
		}
	}
};
}

std::unique_ptr<Machine::DynamicMachine> Machine::MachineForTarget(
	const Analyser::Static::Target *const target,
	const ROMMachine::ROMFetcher &rom_fetcher,
	Machine::Error &error
) {
	error = Machine::Error::None;
	MachineGenerator generator(target, rom_fetcher);

	try {
		using enum Analyser::Machine;
		generator.posit<Amiga::Machine>(Amiga);
		generator.posit<AmstradCPC::Machine>(AmstradCPC);
		generator.posit<Archimedes::Machine>(Archimedes);
		generator.posit<Apple::II::Machine>(AppleII);
		generator.posit<Apple::IIgs::Machine>(AppleIIgs);
		generator.posit<Apple::Macintosh::Machine>(Macintosh);
		generator.posit<Atari2600::Machine>(Atari2600);
		generator.posit<Atari::ST::Machine>(AtariST);
		generator.posit<BBCMicro::Machine>(BBCMicro);
		generator.posit<Coleco::Vision::Machine>(ColecoVision);
		generator.posit<Commodore::Plus4::Machine>(Plus4);
		generator.posit<Commodore::Vic20::Machine>(Vic20);
		generator.posit<Electron::Machine>(Electron);
		generator.posit<Enterprise::Machine>(Enterprise);
		generator.posit<MSX::Machine>(MSX);
		generator.posit<Oric::Machine>(Oric);
		generator.posit<PCCompatible::Machine>(PCCompatible);
		generator.posit<Sega::MasterSystem::Machine>(MasterSystem);
		generator.posit<Sinclair::ZX8081::Machine>(ZX8081);
		generator.posit<Sinclair::ZXSpectrum::Machine>(ZXSpectrum);
		generator.posit<Tandy::CoCo::Machine>(TandyCoCo);
		generator.posit<Thomson::MO::Machine>(ThomsonMO);

		if(!generator.machine) {
			error = Machine::Error::UnknownMachine;
		}
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

	return std::move(generator.machine);
}

std::unique_ptr<Machine::DynamicMachine> Machine::MachineForTargets(
	const Analyser::Static::TargetList &targets,
	const ROMMachine::ROMFetcher &rom_fetcher,
	Error &error
) {
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
		case Analyser::Machine::BBCMicro:		return "BBCMicro";
		case Analyser::Machine::ColecoVision:	return "ColecoVision";
		case Analyser::Machine::Electron:		return "Electron";
		case Analyser::Machine::Enterprise:		return "Enterprise";
		case Analyser::Machine::Macintosh:		return "Macintosh";
		case Analyser::Machine::MasterSystem:	return "MasterSystem";
		case Analyser::Machine::MSX:			return "MSX";
		case Analyser::Machine::Oric:			return "Oric";
		case Analyser::Machine::Plus4:			return "Plus4";
		case Analyser::Machine::PCCompatible:	return "PCCompatible";
		case Analyser::Machine::TandyCoCo:		return "TandyCoCo";
		case Analyser::Machine::ThomsonMO:		return "ThomsonMO";
		case Analyser::Machine::Vic20:			return "Vic20";
		case Analyser::Machine::ZX8081:			return "ZX8081";
		case Analyser::Machine::ZXSpectrum:		return "ZXSpectrum";

		default:	return "";
	}
}

std::string Machine::LongNameForTargetMachine(const Analyser::Machine machine) {
	switch(machine) {
		case Analyser::Machine::Amiga:			return "Amiga";
		case Analyser::Machine::AmstradCPC:		return "Amstrad CPC";
		case Analyser::Machine::AppleII:		return "Apple II";
		case Analyser::Machine::AppleIIgs:		return "Apple IIgs";
		case Analyser::Machine::Archimedes:		return "Acorn Archimedes";
		case Analyser::Machine::Atari2600:		return "Atari 2600";
		case Analyser::Machine::AtariST:		return "Atari ST";
		case Analyser::Machine::BBCMicro:		return "BBC Micro";
		case Analyser::Machine::ColecoVision:	return "ColecoVision";
		case Analyser::Machine::Electron:		return "Acorn Electron";
		case Analyser::Machine::Enterprise:		return "Enterprise";
		case Analyser::Machine::Macintosh:		return "Apple Macintosh";
		case Analyser::Machine::MasterSystem:	return "Sega Master System";
		case Analyser::Machine::MSX:			return "MSX";
		case Analyser::Machine::Oric:			return "Oric";
		case Analyser::Machine::Plus4:			return "Commodore C16+4";
		case Analyser::Machine::PCCompatible:	return "PC Compatible";
		case Analyser::Machine::TandyCoCo:		return "Tandy CoCo";
		case Analyser::Machine::ThomsonMO:		return "Thomson MO";
		case Analyser::Machine::Vic20:			return "Vic 20";
		case Analyser::Machine::ZX8081:			return "ZX80/81";
		case Analyser::Machine::ZXSpectrum:		return "ZX Spectrum";

		default:	return "";
	}
}

std::vector<std::string> Machine::AllMachines(const Type type, const bool long_names) {
	std::vector<std::string> result;
	const auto add_name = [&](const Analyser::Machine machine) {
		result.push_back(
			long_names ? LongNameForTargetMachine(machine) : ShortNameForTargetMachine(machine)
		);
	};

	if(type == Type::Any || type == Type::RequiresMedia) {
		add_name(Analyser::Machine::Atari2600);
		add_name(Analyser::Machine::ColecoVision);
		add_name(Analyser::Machine::MasterSystem);
	}

	if(type == Type::Any || type == Type::DoesntRequireMedia) {
		add_name(Analyser::Machine::Amiga);
		add_name(Analyser::Machine::AmstradCPC);
		add_name(Analyser::Machine::AppleII);
		add_name(Analyser::Machine::AppleIIgs);
		add_name(Analyser::Machine::Archimedes);
		add_name(Analyser::Machine::AtariST);
		add_name(Analyser::Machine::BBCMicro);
		add_name(Analyser::Machine::Electron);
		add_name(Analyser::Machine::Enterprise);
		add_name(Analyser::Machine::Macintosh);
		add_name(Analyser::Machine::MSX);
		add_name(Analyser::Machine::Oric);
		add_name(Analyser::Machine::Plus4);
		add_name(Analyser::Machine::PCCompatible);
		add_name(Analyser::Machine::TandyCoCo);
		add_name(Analyser::Machine::ThomsonMO);
		add_name(Analyser::Machine::Vic20);
		add_name(Analyser::Machine::ZX8081);
		add_name(Analyser::Machine::ZXSpectrum);
	}

	return result;
}

namespace {
struct OptionsList {
	std::map<std::string, std::unique_ptr<Reflection::Struct>> options;

	template <typename MachineT>
	void emplace(const Analyser::Machine machine) {
		options.emplace(
			Machine::LongNameForTargetMachine(machine),
			std::make_unique<typename MachineT::Options>(Configurable::OptionsType::UserFriendly)
		);
	};
};
}

std::map<std::string, std::unique_ptr<Reflection::Struct>> Machine::AllOptionsByMachineName() {
	OptionsList options;

	using enum Analyser::Machine;
	options.emplace<AmstradCPC::Machine>(AmstradCPC);
	options.emplace<Apple::II::Machine>(AppleII);
	options.emplace<Archimedes::Machine>(Archimedes);
	options.emplace<Atari::ST::Machine>(AtariST);
	options.emplace<BBCMicro::Machine>(BBCMicro);
	options.emplace<Coleco::Vision::Machine>(ColecoVision);
	options.emplace<Electron::Machine>(Electron);
	options.emplace<Enterprise::Machine>(Enterprise);
	options.emplace<Apple::Macintosh::Machine>(Macintosh);
	options.emplace<Sega::MasterSystem::Machine>(MasterSystem);
	options.emplace<MSX::Machine>(MSX);
	options.emplace<Oric::Machine>(Oric);
	options.emplace<Commodore::Plus4::Machine>(Plus4);
	options.emplace<PCCompatible::Machine>(PCCompatible);
	options.emplace<Thomson::MO::Machine>(ThomsonMO);
	options.emplace<Commodore::Vic20::Machine>(Vic20);
	options.emplace<Sinclair::ZX8081::Machine>(ZX8081);
	options.emplace<Sinclair::ZXSpectrum::Machine>(ZXSpectrum);

	return std::move(options.options);
}

namespace {
struct TargetList {
	std::map<std::string, std::unique_ptr<Analyser::Static::Target>> targets;

	template <typename TargetT>
	void emplace(const Analyser::Machine machine) {
		targets.emplace(
			Machine::LongNameForTargetMachine(machine),
			std::make_unique<TargetT>()
		);
	};

	void emplace(const Analyser::Machine machine, std::unique_ptr<Analyser::Static::Target> &&target) {
		targets.emplace(
			Machine::LongNameForTargetMachine(machine),
			std::move(target)
		);
	}
};
}

std::map<std::string, std::unique_ptr<Analyser::Static::Target>> Machine::TargetsByMachineName(
	const bool meaningful_without_media_only
) {
	TargetList targets;

	using enum Analyser::Machine;
	targets.emplace<Analyser::Static::Amiga::Target>(Amiga);
	targets.emplace<Analyser::Static::AmstradCPC::Target>(AmstradCPC);
	targets.emplace<Analyser::Static::AppleII::Target>(AppleII);
	targets.emplace<Analyser::Static::AppleIIgs::Target>(AppleIIgs);
	targets.emplace<Analyser::Static::Acorn::ArchimedesTarget>(Archimedes);
	targets.emplace<Analyser::Static::AtariST::Target>(AtariST);
	targets.emplace<Analyser::Static::Acorn::BBCMicroTarget>(BBCMicro);
	targets.emplace<Analyser::Static::Acorn::ElectronTarget>(Electron);
	targets.emplace<Analyser::Static::Enterprise::Target>(Enterprise);
	targets.emplace<Analyser::Static::Macintosh::Target>(Macintosh);
	targets.emplace<Analyser::Static::MSX::Target>(MSX);
	targets.emplace<Analyser::Static::Oric::Target>(Oric);
	targets.emplace<Analyser::Static::Commodore::Plus4Target>(Plus4);
	targets.emplace<Analyser::Static::PCCompatible::Target>(PCCompatible);
	targets.emplace<Analyser::Static::Thomson::MOTarget>(ThomsonMO);
	targets.emplace<Analyser::Static::Commodore::Vic20Target>(Vic20);
	targets.emplace<Analyser::Static::ZX8081::Target>(ZX8081);
	targets.emplace<Analyser::Static::ZXSpectrum::Target>(ZXSpectrum);

	if(!meaningful_without_media_only) {
		targets.emplace<Analyser::Static::Atari2600::Target>(Atari2600);
		targets.emplace(
			ColecoVision,
			std::make_unique<Analyser::Static::Target>(Analyser::Machine::ColecoVision)
		);
		targets.emplace<Analyser::Static::Sega::Target>(MasterSystem);
	}

	return std::move(targets.targets);
}
