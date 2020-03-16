//
//  Electron.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/01/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#ifndef Electron_hpp
#define Electron_hpp

#include "../../Reflection/Struct.h"
#include "../../Analyser/Static/StaticAnalyser.hpp"
#include "../ROMMachine.hpp"

#include <memory>

namespace Electron {

/// @returns The options available for an Electron.
std::unique_ptr<Reflection::Struct> get_options();

/*!
	@abstract Represents an Acorn Electron.

	@discussion An instance of Electron::Machine represents the current state of an
	Acorn Electron.
*/
class Machine {
	public:
		virtual ~Machine();

		/// Creates and returns an Electron.
		static Machine *Electron(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher);
};

}

#endif /* Electron_hpp */
