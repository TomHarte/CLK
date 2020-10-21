//
//  AppleIIgs.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/10/2020.
//  Copyright 2020 Thomas Harte. All rights reserved.
//

#ifndef AppleIIgs_hpp
#define AppleIIgs_hpp

#include "../../../Configurable/Configurable.hpp"
#include "../../../Configurable/StandardOptions.hpp"
#include "../../../Analyser/Static/StaticAnalyser.hpp"
#include "../../ROMMachine.hpp"

#include <memory>

namespace Apple {
namespace IIgs {

class Machine {
	public:
		virtual ~Machine();

		/// Creates and returns an AppleIIgs.
		static Machine *AppleIIgs(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher);
};

}
}

#endif /* AppleII_hpp */
