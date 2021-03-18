//
//  ZXSpectrum.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/03/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "ZXSpectrum.hpp"

#include "../../MachineTypes.hpp"

#include "../../../Analyser/Static/StaticAnalyser.hpp"


using namespace Sinclair::ZXSpectrum;

Machine *Machine::ZXSpectrum(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher) {
	/* TODO */
	(void)target;
	(void)rom_fetcher;
	return nullptr;
}

Machine::~Machine() {}
