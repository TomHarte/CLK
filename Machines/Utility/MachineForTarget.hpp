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
	Returns a map from machine name to the list of options that machine
	exposes, for all machines.
*/
std::map<std::string, std::vector<std::unique_ptr<Configurable::Option>>> AllOptionsByMachineName();

std::map<std::string, std::unique_ptr<Reflection::Struct>> ConstructionOptionsByMachineName();

}

#endif /* MachineForTarget_hpp */
