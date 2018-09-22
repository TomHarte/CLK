//
//  MasterSystem.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/09/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#ifndef MasterSystem_hpp
#define MasterSystem_hpp

#include "../../Analyser/Static/StaticAnalyser.hpp"
#include "../ROMMachine.hpp"

namespace Sega {
namespace MasterSystem {

class Machine {
	public:
		virtual ~Machine();
		static Machine *MasterSystem(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher);
};

}
}

#endif /* MasterSystem_hpp */
