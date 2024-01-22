//
//  StaticAnalyser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/10/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"
#include "Target.hpp"

Analyser::Static::TargetList Analyser::Static::PCCompatible::GetTargets(const Media &media, const std::string &, TargetPlatform::IntType) {
	// This analyser can comprehend disks only.
	if(media.disks.empty()) return {};

	// No analysis is applied yet.
	Analyser::Static::TargetList targets;

	using Target = Analyser::Static::PCCompatible::Target;
	auto *const target = new Target();
	target->media = media;
	targets.push_back(std::unique_ptr<Analyser::Static::Target>(target));

	return targets;
}
