//
//  StaticAnalyser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"

#include <string>
#include <vector>

#include "Target.hpp"
#include "../../../Storage/Tape/Parsers/ZX8081.hpp"

static std::vector<Storage::Data::ZX8081::File> GetFiles(Storage::Tape::TapeSerialiser &serialiser) {
	std::vector<Storage::Data::ZX8081::File> files;
	Storage::Tape::ZX8081::Parser parser;

	while(!serialiser.is_at_end()) {
		std::shared_ptr<Storage::Data::ZX8081::File> next_file = parser.get_next_file(serialiser);
		if(next_file != nullptr) {
			files.push_back(*next_file);
		}
	}

	return files;
}

Analyser::Static::TargetList Analyser::Static::ZX8081::GetTargets(
	const Media &media,
	const std::string &,
	TargetPlatform::IntType potential_platforms,
	bool
) {
	TargetList destination;
	if(!media.tapes.empty()) {
		const auto serialiser = media.tapes.front()->serialiser();
		std::vector<Storage::Data::ZX8081::File> files = GetFiles(*serialiser);
		if(!files.empty()) {
			Target *const target = new Target;
			destination.push_back(std::unique_ptr<::Analyser::Static::Target>(target));
			target->machine = Machine::ZX8081;

			// Guess the machine type from the file only if it isn't already known.
			switch(potential_platforms & (TargetPlatform::ZX80 | TargetPlatform::ZX81)) {
				default:
					target->is_ZX81 = files.front().isZX81;
				break;

				case TargetPlatform::ZX80:	target->is_ZX81 = false;	break;
				case TargetPlatform::ZX81:	target->is_ZX81 = true;		break;
			}

			/*if(files.front().data.size() > 16384) {
				target->zx8081.memory_model = ZX8081MemoryModel::SixtyFourKB;
			} else*/ if(files.front().data.size() > 1024) {
				target->memory_model = Target::MemoryModel::SixteenKB;
			} else {
				target->memory_model = Target::MemoryModel::Unexpanded;
			}
			target->media.tapes = media.tapes;

			// TODO: how to run software once loaded? Might require a BASIC detokeniser.
			if(target->is_ZX81) {
				target->loading_command = "J\"\"\n";
			} else {
				target->loading_command = "W\n";
			}

		}
	}
	return destination;
}
