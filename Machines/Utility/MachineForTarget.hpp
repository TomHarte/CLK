//
//  MachineForTarget.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/11/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#pragma once

#include "../../Analyser/Static/StaticAnalyser.hpp"
#include "../../Reflection/Struct.hpp"

#include "../DynamicMachine.hpp"
#include "../ROMMachine.hpp"

#include <map>
#include <memory>
#include <string>
#include <vector>

/*!
	This namespace acts as a grab-bag of functions that allow a client to:

		(i) discover the total list of implemented machines;
		(ii) discover the construction and runtime options available for controlling them; and
		(iii) create any implemented machine via its construction options.

	See Reflection::Struct and Reflection::Enum for getting dynamic information from the
	Targets that this namespace deals in.
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
std::unique_ptr<DynamicMachine> MachineForTargets(const Analyser::Static::TargetList &targets, const ::ROMMachine::ROMFetcher &rom_fetcher, Error &error);

/*!
	Allocates an instance of DynamicMaachine holding the machine described
	by @c target. It is the caller's responsibility to delete the class when finished.
*/
std::unique_ptr<DynamicMachine> MachineForTarget(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher, Machine::Error &error);

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

enum class Type {
	RequiresMedia,
	DoesntRequireMedia,
	Any
};

/*!
	@param type the type of machines to include.
	@param long_names If this is @c true then long names will be returned; otherwise short names will be returned.

	@returns A list of all available machines. Names are always guaranteed to be in the same order.
*/
std::vector<std::string> AllMachines(Type type, bool long_names);

/*!
	Returns a map from long machine name to the list of options that machine exposes, for all machines.
	In all cases, user-friendly selections will have been filled in by default.
*/
std::map<std::string, std::unique_ptr<Reflection::Struct>> AllOptionsByMachineName();

/*!
	Returns a map from long machine name to appropriate instances of Target for the machine.

	NB: Usually the instances of Target can be dynamic_casted to Reflection::Struct in order to determine available properties.
*/
std::map<std::string, std::unique_ptr<Analyser::Static::Target>> TargetsByMachineName(bool meaningful_without_media_only);

}
