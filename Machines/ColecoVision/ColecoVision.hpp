//
//  ColecoVision.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/02/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#ifndef ColecoVision_hpp
#define ColecoVision_hpp

#include "../../Analyser/Static/StaticAnalyser.hpp"
#include "../ROMMachine.hpp"

namespace Coleco {
namespace Vision {

class Machine {
	public:
		virtual ~Machine();
		static Machine *ColecoVision(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher);
};

}
}

#endif /* ColecoVision_hpp */
