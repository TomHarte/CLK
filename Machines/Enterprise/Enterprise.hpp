//
//  Enterprise.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/06/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef Enterprise_hpp
#define Enterprise_hpp

#include "../../Analyser/Static/StaticAnalyser.hpp"
#include "../ROMMachine.hpp"

namespace Enterprise {

class Machine {
	public:
		virtual ~Machine();

		static Machine *Enterprise(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher);

};

};

#endif /* Enterprise_hpp */
