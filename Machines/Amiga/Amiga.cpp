//
//  Amiga.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/07/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "Amiga.hpp"

#include "../../Analyser/Static/Amiga/Target.hpp"

namespace Amiga {

class ConcreteMachine:
	public Machine {
	public:
		ConcreteMachine(const Analyser::Static::Amiga::Target &target, const ROMMachine::ROMFetcher &rom_fetcher) {
			(void)target;
			(void)rom_fetcher;
		}

};

}


using namespace Amiga;

Machine *Machine::Amiga(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher) {
	using Target = Analyser::Static::Amiga::Target;
	const Target *const amiga_target = dynamic_cast<const Target *>(target);
	return new Amiga::ConcreteMachine(*amiga_target, rom_fetcher);
}

Machine::~Machine() {}
