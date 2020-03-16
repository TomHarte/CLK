//
//  MasterSystem.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/09/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#ifndef MasterSystem_hpp
#define MasterSystem_hpp

#include "../../Reflection/Struct.h"
#include "../../Analyser/Static/StaticAnalyser.hpp"
#include "../ROMMachine.hpp"

#include <memory>

namespace Sega {
namespace MasterSystem {

std::unique_ptr<Reflection::Struct> get_options();

class Machine {
	public:
		virtual ~Machine();
		static Machine *MasterSystem(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher);
};

}
}

#endif /* MasterSystem_hpp */
