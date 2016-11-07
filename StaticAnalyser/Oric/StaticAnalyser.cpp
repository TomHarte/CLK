//
//  StaticAnalyser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 11/10/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"

#include "Tape.hpp"

using namespace StaticAnalyser::Oric;

void StaticAnalyser::Oric::AddTargets(
	const std::list<std::shared_ptr<Storage::Disk::Disk>> &disks,
	const std::list<std::shared_ptr<Storage::Tape::Tape>> &tapes,
	const std::list<std::shared_ptr<Storage::Cartridge::Cartridge>> &cartridges,
	std::list<StaticAnalyser::Target> &destination)
{
	Target target;
	target.machine = Target::Oric;
	target.probability = 1.0;

	for(auto tape : tapes)
	{
		std::list<File> tape_files = GetFiles(tape);
		if(tape_files.size())
		{
			target.tapes.push_back(tape);
			target.loadingCommand = "CLOAD\"\"\n";
		}
	}

	if(target.tapes.size() || target.disks.size() || target.cartridges.size())
		destination.push_back(target);
}
