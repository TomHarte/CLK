//
//  MachineForTarget.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/11/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "MachineForTarget.hpp"

#include <algorithm>
#include "Machines/Registry.hpp"

#include "Analyser/Dynamic/MultiMachine/MultiMachine.hpp"
#include "TypedDynamicMachine.hpp"

// TODO: incorporate these names into the register.
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

namespace {
struct MachineGenerator {
	MachineGenerator(
		const Analyser::Static::Target &target,
		const ROMMachine::ROMFetcher &rom_fetcher
	) : target(target), rom_fetcher(rom_fetcher) {}

	std::unique_ptr<Machine::DynamicMachine> machine;

	template <typename MachineT>
	struct Adder {
		void operator()(MachineGenerator &generator) {
			generator.posit<typename MachineT::Machine>(MachineT::name);
		}
	};

private:
	const Analyser::Static::Target &target;
	const ROMMachine::ROMFetcher &rom_fetcher;
	template <typename MachineT>
	void posit(const Analyser::Machine name) {
		if(target.machine == name) {
			machine = std::make_unique<Machine::TypedDynamicMachine<MachineT>>(MachineT::create(target, rom_fetcher));
		}
	}
};
}

std::unique_ptr<Machine::DynamicMachine> Machine::MachineForTarget(
	const Analyser::Static::Target &target,
	const ROMMachine::ROMFetcher &rom_fetcher,
	Machine::Error &error
) {
	error = Machine::Error::None;
	MachineGenerator generator(target, rom_fetcher);

	try {
		MachineRegister::for_all_machines<MachineGenerator::Adder>(generator);
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
			machines.emplace_back(MachineForTarget(*target, rom_fetcher, error));

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
	return MachineForTarget(*targets.front(), rom_fetcher, error);
}

namespace {
struct MachineLister {
	MachineLister(const Machine::Type type, const bool long_names) :
		type_(type), long_names_(long_names) {};

	std::vector<std::string> machines;

	template <typename MachineT>
	struct Adder {
		void operator()(MachineLister &lister) {
			switch(lister.type_) {
				case Machine::Type::RequiresMedia:
					if(!MachineT::requires_media) return;
				break;

				case Machine::Type::DoesntRequireMedia:
					if(MachineT::requires_media) return;
				break;

				default: break;
			}

			lister.machines.push_back(lister.long_names_ ?
				Machine::LongNameForTargetMachine(MachineT::name) : Machine::ShortNameForTargetMachine(MachineT::name)
			);
		}
	};

private:
	Machine::Type type_;
	bool long_names_;
};

}

std::vector<std::string> Machine::AllMachines(const Type type, const bool long_names) {
	MachineLister lister(type, long_names);
	MachineRegister::for_all_machines<MachineLister::Adder>(lister);
	return std::move(lister.machines);
}

namespace {
struct OptionsList {
	std::map<std::string, std::unique_ptr<Reflection::Struct>> options;

	template <typename MachineT>
	struct Adder {
		void operator()(OptionsList &list) {
			list.emplace<typename MachineT::Machine>(MachineT::name);
		}
	};

private:
	template <typename MachineT>
	void emplace(const Analyser::Machine machine) {
		if constexpr (requires{ MachineT::Options(Configurable::OptionsType()); }) {
			options.emplace(
				Machine::LongNameForTargetMachine(machine),
				std::make_unique<typename MachineT::Options>(Configurable::OptionsType::UserFriendly)
			);
		}
	};
};
}

std::map<std::string, std::unique_ptr<Reflection::Struct>> Machine::AllOptionsByMachineName() {
	OptionsList options;
	MachineRegister::for_all_machines<OptionsList::Adder>(options);
	return std::move(options.options);
}

namespace {
struct TargetList {
	TargetList(const bool meaningful_without_media_only) :
		meaningful_without_media_only_(meaningful_without_media_only) {}

	std::map<std::string, std::unique_ptr<Analyser::Static::Target>> targets;

	template <typename MachineT>
	struct Adder {
		void operator()(TargetList &list) {
			if(MachineT::requires_media && list.meaningful_without_media_only_) {
				return;
			}

			if constexpr (requires { typename MachineT::Target(); }) {
				list.emplace<typename MachineT::Target>(MachineT::name);
			} else {
				list.emplace(
					MachineT::name,
					std::make_unique<Analyser::Static::Target>(MachineT::name)
				);
			}
		}
	};

private:
	bool meaningful_without_media_only_;

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
	TargetList targets(meaningful_without_media_only);
	MachineRegister::for_all_machines<TargetList::Adder>(targets);
	return std::move(targets.targets);
}
