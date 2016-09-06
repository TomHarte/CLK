//
//  CommodoreAnalyser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/09/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "CommodoreAnalyser.hpp"

#include "Tape.hpp"

using namespace StaticAnalyser::Commodore;

void StaticAnalyser::Commodore::AddTargets(
	const std::list<std::shared_ptr<Storage::Disk::Disk>> &disks,
	const std::list<std::shared_ptr<Storage::Tape::Tape>> &tapes,
	const std::list<std::shared_ptr<Storage::Cartridge::Cartridge>> &cartridges,
	std::list<StaticAnalyser::Target> &destination)
{
	Target target;
	target.machine = Target::Vic20;	// TODO: machine estimation
	target.probability = 1.0; // TODO: a proper estimation

	// strip out inappropriate cartridges
//	target.cartridges = AcornCartridgesFrom(cartridges);

	// if there are any tapes, attempt to get data from the first
	if(tapes.size() > 0)
	{
		std::shared_ptr<Storage::Tape::Tape> tape = tapes.front();
		tape->reset();
		std::list<File> files = GetFiles(tape);
		tape->reset();
	}
}