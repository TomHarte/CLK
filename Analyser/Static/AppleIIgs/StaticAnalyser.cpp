//
//  StaticAnalyser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/10/2020.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"
#include "Target.hpp"

Analyser::Static::TargetList Analyser::Static::AppleIIgs::GetTargets(const Media &media, const std::string &, TargetPlatform::IntType) {
	auto target = std::make_unique<Target>();
	target->media = media;

	TargetList targets;
	targets.push_back(std::move(target));
	return targets;
}
