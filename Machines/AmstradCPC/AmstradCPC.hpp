//
//  AmstradCPC.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 30/07/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef AmstradCPC_hpp
#define AmstradCPC_hpp

#include "../../Reflection/Struct.h"
#include "../../Analyser/Static/StaticAnalyser.hpp"
#include "../ROMMachine.hpp"

#include <memory>

namespace AmstradCPC {

/// @returns The options available for an Amstrad CPC.
std::unique_ptr<Reflection::Struct> get_options();

/*!
	Models an Amstrad CPC.
*/
class Machine {
	public:
		virtual ~Machine();

		/// Creates and returns an Amstrad CPC.
		static Machine *AmstradCPC(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher);
};

}

#endif /* AmstradCPC_hpp */
