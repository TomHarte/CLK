//
//  Vic20.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#ifndef Vic20_hpp
#define Vic20_hpp

#include "../../../Reflection/Struct.h"
#include "../../../Analyser/Static/StaticAnalyser.hpp"
#include "../../ROMMachine.hpp"

#include <memory>

namespace Commodore {
namespace Vic20 {

/// @returns The options available for a Vic-20.
std::unique_ptr<Reflection::Struct> get_options();

class Machine {
	public:
		virtual ~Machine();

		/// Creates and returns a Vic-20.
		static Machine *Vic20(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher);
};

}
}

#endif /* Vic20_hpp */
