//
//  Enterprise.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/06/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "Enterprise.hpp"

#include "../../Analyser/Static/Enterprise/Target.hpp"

namespace Enterprise {

class ConcreteMachine:
	public Machine {
	public:
		ConcreteMachine(const Analyser::Static::Enterprise::Target &target, const ROMMachine::ROMFetcher &rom_fetcher) {
			(void)target;
			(void)rom_fetcher;
		}
};

}

using namespace Enterprise;

Machine *Machine::Enterprise(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher) {
	using Target = Analyser::Static::Enterprise::Target;
	const Target *const enterprise_target = dynamic_cast<const Target *>(target);

	return new Enterprise::ConcreteMachine(*enterprise_target, rom_fetcher);
}

Machine::~Machine() {}
