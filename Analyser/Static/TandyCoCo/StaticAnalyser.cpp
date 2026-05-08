//
//  StaticAnalyser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 08/05/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"

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
	targets.push_back(std::move(target));
	return targets;
}
