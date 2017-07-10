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

static std::vector<Storage::Data::ZX8081::File> GetFiles(const std::shared_ptr<Storage::Tape::Tape> &tape) {
	std::vector<Storage::Data::ZX8081::File> files;
	Storage::Tape::ZX8081::Parser parser;

	while(!tape->is_at_end()) {
		std::shared_ptr<Storage::Data::ZX8081::File> next_file = parser.get_next_file(tape);
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
		std::vector<Storage::Data::ZX8081::File> files = GetFiles(tapes.front());
		if(!files.empty()) {
			StaticAnalyser::Target target;
			target.machine = Target::ZX8081;
			target.zx8081.isZX81 = files.front().isZX81;
			if(files.front().data.size() > 16384) {
				target.zx8081.memory_model = ZX8081MemoryModel::SixtyFourKB;
			} else if(files.front().data.size() > 1024) {
				target.zx8081.memory_model = ZX8081MemoryModel::SixteenKB;
			} else {
				target.zx8081.memory_model = ZX8081MemoryModel::Unexpanded;
			}
			target.tapes = tapes;

			// TODO: how to run software once loaded? Might require a BASIC detokeniser.
			if(target.zx8081.isZX81) {
				target.loadingCommand = "J\"\"\n";
			} else {
				target.loadingCommand = "W\n";
			}

			destination.push_back(target);
		}
	}
}
