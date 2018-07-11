//
//  Electron.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/01/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#ifndef Electron_hpp
#define Electron_hpp

#include "../../Configurable/Configurable.hpp"
#include "../../Analyser/Static/StaticAnalyser.hpp"
#include "../ROMMachine.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace Electron {

/// @returns The options available for an Electron.
std::vector<std::unique_ptr<Configurable::Option>> get_options();

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
