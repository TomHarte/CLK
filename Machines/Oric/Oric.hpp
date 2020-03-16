//
//  Oric.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 11/10/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#ifndef Oric_hpp
#define Oric_hpp

#include "../../Reflection/Struct.h"
#include "../../Analyser/Static/StaticAnalyser.hpp"
#include "../ROMMachine.hpp"

#include <memory>

namespace Oric {

/// @returns The options available for an Oric.
std::unique_ptr<Reflection::Struct> get_options();

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
