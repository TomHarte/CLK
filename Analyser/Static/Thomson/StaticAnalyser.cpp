//
//  StaticAnalyser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/03/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"

Analyser::Static::TargetList Analyser::Static::Thomson::GetTargets(
	const Media &media,
	const std::string &,
	TargetPlatform::IntType,
	bool
) {
	// No discernment here; assume a Thomson MO because it's all I have.
	TargetList destination;
	auto target = std::make_unique<Target>(Machine::ThomsonMO);
	target->media = media;
	destination.push_back(std::move(target));
	return destination;
}
