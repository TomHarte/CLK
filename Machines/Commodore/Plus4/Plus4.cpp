//
//  Plus4.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/12/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#include "Plus4.hpp"

using namespace Commodore::Plus4;

std::unique_ptr<Machine> Machine::Plus4(
	const Analyser::Static::Target *target,
	const ROMMachine::ROMFetcher &rom_fetcher
) {
	(void)target;
	(void)rom_fetcher;
	return nullptr;
}
