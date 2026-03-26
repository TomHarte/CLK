//
//  StaticAnalyser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/03/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"
#include "Target.hpp"

#include "Storage/Tape/Parsers/ThomsonMO.hpp"

Analyser::Static::TargetList Analyser::Static::Thomson::GetTargets(
	const Media &media,
	const std::string &,
	TargetPlatform::IntType,
	bool
) {
	using MOTarget = Analyser::Static::Thomson::MOTarget;

	TargetList destination;
	auto target = std::make_unique<MOTarget>();

	if(!media.tapes.empty()) {
		Storage::Tape::Thomson::MO::Parser parser;
		auto &tape = media.tapes.front();
		const auto serialiser = tape->serialiser();
		const auto first = parser.block(*serialiser);

		if(first && first->checksum_valid) {
			target->media.tapes = media.tapes;
		}
	}

	if(!target->media.empty()) {
		destination.push_back(std::move(target));
	}
	return destination;
}
