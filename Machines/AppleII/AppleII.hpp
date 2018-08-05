//
//  AppleII.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/04/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#ifndef AppleII_hpp
#define AppleII_hpp

#include "../../Configurable/Configurable.hpp"
#include "../../Analyser/Static/StaticAnalyser.hpp"
#include "../ROMMachine.hpp"

#include <memory>
#include <vector>

namespace AppleII {

class Machine {
	public:
		virtual ~Machine();

		/// Creates and returns an AppleII.
		static Machine *AppleII(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher);
};

};

#endif /* AppleII_hpp */
