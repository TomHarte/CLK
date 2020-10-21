//
//  AppleIIgs.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/10/2020.
//  Copyright 2020 Thomas Harte. All rights reserved.
//

#include "AppleIIgs.hpp"

#include "../../../Analyser/Static/AppleIIgs/Target.hpp"

namespace Apple {
namespace IIgs {

}
}

using namespace Apple::IIgs;

Machine *Machine::AppleIIgs(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher) {
	using Target = Analyser::Static::AppleIIgs::Target;

	// TODO.
	(void)target;
	(void)rom_fetcher;

	return nullptr;
}

Machine::~Machine() {}
