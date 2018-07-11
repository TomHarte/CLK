//
//  Oric.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 11/10/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#ifndef Oric_hpp
#define Oric_hpp

#include "../../Configurable/Configurable.hpp"
#include "../../Analyser/Static/StaticAnalyser.hpp"
#include "../ROMMachine.hpp"

namespace Oric {

/// @returns The options available for an Oric.
std::vector<std::unique_ptr<Configurable::Option>> get_options();

/*!
	Models an Oric 1/Atmos with or without a Microdisc.
*/
class Machine {
	public:
		virtual ~Machine();

		/// Creates and returns an Oric.
		static Machine *Oric(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher);
};

}
#endif /* Oric_hpp */
