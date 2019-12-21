//
//  AtariST.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/10/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef AtariST_hpp
#define AtariST_hpp

#include "../../../Configurable/Configurable.hpp"
#include "../../../Analyser/Static/StaticAnalyser.hpp"
#include "../../ROMMachine.hpp"

namespace Atari {
namespace ST {

/// @returns The options available for an Atari ST.
std::vector<std::unique_ptr<Configurable::Option>> get_options();

class Machine {
	public:
		virtual ~Machine();

		static Machine *AtariST(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher);
};

}
}
#endif /* AtariST_hpp */
