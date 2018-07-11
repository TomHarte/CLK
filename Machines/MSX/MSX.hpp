//
//  MSX.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 24/11/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef MSX_hpp
#define MSX_hpp

#include "../../Configurable/Configurable.hpp"
#include "../../Analyser/Static/StaticAnalyser.hpp"
#include "../ROMMachine.hpp"

#include <memory>
#include <vector>

namespace MSX {

std::vector<std::unique_ptr<Configurable::Option>> get_options();

class Machine {
	public:
		virtual ~Machine();
		static Machine *MSX(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher);
};

}

#endif /* MSX_hpp */
