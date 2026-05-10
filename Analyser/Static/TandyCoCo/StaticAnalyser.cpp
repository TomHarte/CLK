//
//  StaticAnalyser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 08/05/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"

#include "Storage/Tape/Parsers/TandyCoCo.hpp"

#include "Target.hpp"

Analyser::Static::TargetList Analyser::Static::TandyCoCo::GetTargets(
	const Media &media,
	const std::string &,
	TargetPlatform::IntType,
	bool
) {
	TargetList targets;
	auto target = std::make_unique<Target>();
	target->media = media;

	if(!media.tapes.empty()) {
		using namespace Storage::Tape::TandyCoCo;

		Parser parser;
		auto &tape = media.tapes.front();
		const auto serialiser = tape->serialiser();

		while(!serialiser->is_at_end()) {
			const auto block = parser.block(*serialiser);
			if(!block.has_value()) continue;

			if(!block->type && block->data.size() >= 9) {
				if(!block->data[8]) {
					target->loading_command = L"CLOAD:RUN\n";
				} else {
					target->loading_command = L"CLOADM:EXEC\n";
				}
				break;
			}
		}
	}

	targets.push_back(std::move(target));
	return targets;
}
