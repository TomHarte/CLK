//
//  CoCo.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 01/05/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include "CoCo.hpp"

using namespace Tandy::CoCo;

std::unique_ptr<Machine> Machine::create(
	const Analyser::Static::Target &,
	const ROMMachine::ROMFetcher &
) {
	return nullptr;
}
