//
//  StaticAnalyser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"

#include <string>
#include <vector>

#include "../../Storage/Tape/Parsers/ZX8081.hpp"

static std::vector<Storage::Tape::ZX8081::File> GetFiles(const std::shared_ptr<Storage::Tape::Tape> &tape) {
	std::vector<Storage::Tape::ZX8081::File> files;
	Storage::Tape::ZX8081::Parser parser;

	while(!tape->is_at_end()) {
		std::shared_ptr<Storage::Tape::ZX8081::File> next_file = parser.get_next_file(tape);
		if(next_file != nullptr) {
			files.push_back(*next_file);
		}
	}

	return files;
}

void StaticAnalyser::ZX8081::AddTargets(
		const std::list<std::shared_ptr<Storage::Disk::Disk>> &disks,
		const std::list<std::shared_ptr<Storage::Tape::Tape>> &tapes,
		const std::list<std::shared_ptr<Storage::Cartridge::Cartridge>> &cartridges,
		std::list<StaticAnalyser::Target> &destination) {

	if(!tapes.empty()) {
		std::vector<Storage::Tape::ZX8081::File> files = GetFiles(tapes.front());
		if(!files.empty()) {
			// TODO: check files for machine type, memory size.
			StaticAnalyser::Target target;
			target.machine = Target::ZX80;
			target.tapes = tapes;
			destination.push_back(target);
		}
	}
}
