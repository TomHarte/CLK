//
//  StaticAnalyser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"

void StaticAnalyser::ZX8081::AddTargets(
		const std::list<std::shared_ptr<Storage::Disk::Disk>> &disks,
		const std::list<std::shared_ptr<Storage::Tape::Tape>> &tapes,
		const std::list<std::shared_ptr<Storage::Cartridge::Cartridge>> &cartridges,
		std::list<StaticAnalyser::Target> &destination) {
	// Temporary: be entirely trusting.
	StaticAnalyser::Target target;
	target.machine = Target::ZX80;
	target.tapes = tapes;
	destination.push_back(target);
}
