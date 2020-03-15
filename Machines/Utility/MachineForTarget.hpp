//
//  MachineForTarget.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/11/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef MachineForTarget_hpp
#define MachineForTarget_hpp

#include "../../Analyser/Static/StaticAnalyser.hpp"
#include "../../Reflection/Struct.h"

#include "../DynamicMachine.hpp"

#include <map>
#include <memory>
#include <string>
#include <vector>

/*!
*/
namespace Machine {

enum class Error {
	None,
	UnknownError,
	UnknownMachine,
	MissingROM,
	NoTargets
};

/*!
	Allocates an instance of DynamicMachine holding a machine that can
	receive the supplied static analyser result. The machine has been allocated
	on the heap. It is the caller's responsibility to delete the class when finished.
*/
DynamicMachine *MachineForTargets(const Analyser::Static::TargetList &targets, const ::ROMMachine::ROMFetcher &rom_fetcher, Error &error);

/*!
	Allocates an instance of DynamicMaachine holding the machine described
	by @c target. It is the caller's responsibility to delete the class when finished.
*/
DynamicMachine *MachineForTarget(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher, Machine::Error &error);

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
	@returns A list of all available machines.
*/
std::vector<std::string> AllMachines(bool meaningful_without_media_only, bool long_names);

/*!
	Returns a map from long machine name to the list of options that machine
	exposes, for all machines.
*/
std::map<std::string, std::vector<std::unique_ptr<Configurable::Option>>> AllOptionsByMachineName();

/*!
	Returns a map from long machine name to appropriate instances of Target for the machine.

	NB: Usually the instances of Target can be dynamic_casted to Reflection::Struct in order to determine available properties.
*/
std::map<std::string, std::unique_ptr<Analyser::Static::Target>> TargetsByMachineName(bool meaningful_without_media_only);

}

#endif /* MachineForTarget_hpp */
