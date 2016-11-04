//
//  StaticAnalyser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 11/10/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"

using namespace StaticAnalyser::Oric;

void StaticAnalyser::Oric::AddTargets(
	const std::list<std::shared_ptr<Storage::Disk::Disk>> &disks,
	const std::list<std::shared_ptr<Storage::Tape::Tape>> &tapes,
	const std::list<std::shared_ptr<Storage::Cartridge::Cartridge>> &cartridges,
	std::list<StaticAnalyser::Target> &destination)
{
	// TODO: any sort of sanity checking at all; at the minute just trust the file type
	// approximation already performed.
	Target target;
	target.machine = Target::Oric;
	target.probability = 1.0;
	target.disks = disks;
	target.tapes = tapes;
	target.cartridges = cartridges;
	target.loadingCommand = "CLOAD\"\"\n";
	destination.push_back(target);
}
