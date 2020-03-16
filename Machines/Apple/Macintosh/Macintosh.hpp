//
//  Macintosh.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/05/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef Macintosh_hpp
#define Macintosh_hpp

#include "../../../Reflection/Struct.h"
#include "../../../Analyser/Static/StaticAnalyser.hpp"
#include "../../ROMMachine.hpp"

#include <memory>

namespace Apple {
namespace Macintosh {

std::unique_ptr<Reflection::Struct> get_options();

class Machine {
	public:
		virtual ~Machine();

		/// Creates and returns a Macintosh.
		static Machine *Macintosh(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher);
};


}
}

#endif /* Macintosh_hpp */
