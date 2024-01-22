//
//  Amiga.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/07/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#pragma once

#include "../../Analyser/Static/StaticAnalyser.hpp"
#include "../ROMMachine.hpp"

#include <memory>

namespace Amiga {

class Machine {
	public:
		virtual ~Machine();

		/// Creates and returns an Amiga.
		static std::unique_ptr<Machine> Amiga(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher);
};

}
