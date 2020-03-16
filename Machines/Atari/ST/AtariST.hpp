//
//  AtariST.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/10/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef AtariST_hpp
#define AtariST_hpp

#include "../../../Reflection/Struct.h"
#include "../../../Analyser/Static/StaticAnalyser.hpp"
#include "../../ROMMachine.hpp"

#include <memory>

namespace Atari {
namespace ST {

/// @returns The options available for an Atari ST.
std::unique_ptr<Reflection::Struct> get_options();

class Machine {
	public:
		virtual ~Machine();

		static Machine *AtariST(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher);
};

}
}
#endif /* AtariST_hpp */
