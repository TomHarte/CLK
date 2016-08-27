//
//  StaticAnalyser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/08/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"

using namespace StaticAnalyser;

std::list<Target> GetTargets(std::shared_ptr<Storage::Disk> disk, std::shared_ptr<Storage::Tape> tape, std::shared_ptr<std::vector<uint8_t>> rom)
{
	std::list<Target> targets;

	if(disk)
	{
	}

	if(tape)
	{
	}

	if(rom)
	{
	}

	return targets;
}
