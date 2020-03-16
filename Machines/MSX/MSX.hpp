//
//  MSX.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 24/11/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef MSX_hpp
#define MSX_hpp

#include "../../Reflection/Struct.h"
#include "../../Analyser/Static/StaticAnalyser.hpp"
#include "../ROMMachine.hpp"

#include <memory>

namespace MSX {

std::unique_ptr<Reflection::Struct> get_options();

class Machine {
	public:
		virtual ~Machine();
		static Machine *MSX(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher);
};

}

#endif /* MSX_hpp */
