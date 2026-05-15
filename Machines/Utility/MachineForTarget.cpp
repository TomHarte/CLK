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

namespace {
/*!
	Compares each machine supplied to it to a given target and retains either its short name or its long name
	as per the template parameter.
*/
template <bool capture_short>
struct NameCapture {
	NameCapture(const Analyser::Machine machine) : machine_(machine) {}
	const char *result = "";

	template <typename MachineT>
	struct Adder {
		void operator()(NameCapture &capturer) {
			if(MachineT::name != capturer.machine_) {
				return;
			}
			capturer.result = capture_short ? MachineT::short_name : MachineT::long_name;
		}
	};

private:
	Analyser::Machine machine_;
};

/*
	Creates an instance of Machine::DynamicMachine based on the supplied target and ROM fetcher.
*/
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

/*
	Creates a list naming all complete machines, optionally filtering by launch media requirements.
*/
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

			lister.machines.push_back(lister.long_names_ ? MachineT::long_name : MachineT::short_name);
		}
	};

private:
	Machine::Type type_;
	bool long_names_;
};

/*
	Creates a map from machine long name to default user-friendly options.
*/
struct OptionsList {
	std::map<std::string, std::unique_ptr<Reflection::Struct>> options_by_name;
	std::map<Analyser::Machine, std::unique_ptr<Reflection::Struct>> options_by_machine;

	template <typename MachineT>
	struct Adder {
		void operator()(OptionsList &list) {
			list.emplace<MachineT>();
		}
	};

private:
	template <typename MachineT>
	void emplace() {
		if constexpr (requires{ typename MachineT::Machine::Options(Configurable::OptionsType::UserFriendly); }) {
			options_by_name.emplace(
				MachineT::long_name,
				std::make_unique<typename MachineT::Machine::Options>(Configurable::OptionsType::UserFriendly)
			);
			options_by_machine.emplace(
				MachineT::name,
				std::make_unique<typename MachineT::Machine::Options>(Configurable::OptionsType::UserFriendly)
			);
		}
	};
};

/*
	Creates a map from machine long name to an instantiation of its `Target`, optionally filtered on whether
	media is required for a meaningful launch.
*/
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
				list.emplace<typename MachineT::Target>(MachineT::long_name);
			} else {
				list.emplace(
					MachineT::long_name,
					std::make_unique<Analyser::Static::Target>(MachineT::name)
				);
			}
		}
	};

private:
	bool meaningful_without_media_only_;

	template <typename TargetT>
	void emplace(const char *name) {
		targets.emplace(name, std::make_unique<TargetT>());
	};

	void emplace(const char *name, std::unique_ptr<Analyser::Static::Target> &&target) {
		targets.emplace(name, std::move(target));
	}
};
}

std::string Machine::ShortNameForTargetMachine(const Analyser::Machine machine) {
	NameCapture<true> name(machine);
	MachineRegister::for_all_machines<NameCapture<true>::Adder>(name);
	return name.result;
}

std::string Machine::LongNameForTargetMachine(const Analyser::Machine machine) {
	NameCapture<false> name(machine);
	MachineRegister::for_all_machines<NameCapture<false>::Adder>(name);
	return name.result;
}

std::unique_ptr<Machine::DynamicMachine> Machine::MachineForTarget(
	const Analyser::Static::Target &target,
	const ROMMachine::ROMFetcher &rom_fetcher,
	Machine::Error &error
) {
	error = Machine::Error::None;
	MachineGenerator generator(target, rom_fetcher);

	try {
		MachineRegister::for_all_machines<MachineGenerator::Adder>(generator, true);
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

std::vector<std::string> Machine::AllMachines(const Type type, const bool long_names) {
	MachineLister lister(type, long_names);
	MachineRegister::for_all_machines<MachineLister::Adder>(lister);
	return std::move(lister.machines);
}

std::map<std::string, std::unique_ptr<Reflection::Struct>> Machine::AllOptionsByMachineName() {
	OptionsList options;
	MachineRegister::for_all_machines<OptionsList::Adder>(options);
	return std::move(options.options_by_name);
}

std::map<Analyser::Machine, std::unique_ptr<Reflection::Struct>>
Machine::AllOptionsByMachine(const bool include_incomplete) {
	OptionsList options;
	MachineRegister::for_all_machines<OptionsList::Adder>(options, include_incomplete);
	return std::move(options.options_by_machine);
}

std::map<std::string, std::unique_ptr<Analyser::Static::Target>> Machine::TargetsByMachineName(
	const bool meaningful_without_media_only
) {
	TargetList targets(meaningful_without_media_only);
	MachineRegister::for_all_machines<TargetList::Adder>(targets);
	return std::move(targets.targets);
}
