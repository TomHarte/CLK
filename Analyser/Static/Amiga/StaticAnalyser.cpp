//
//  StaticAnalyser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/07/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"
#include "Target.hpp"

Analyser::Static::TargetList Analyser::Static::Amiga::GetTargets(
	const Media &media,
	const std::string &,
	TargetPlatform::IntType
) {
	// This analyser can comprehend disks and mass-storage devices only.
	if(media.disks.empty()) return {};

	// As there is at least one usable media image, wave it through.
	Analyser::Static::TargetList targets;

	using Target = Analyser::Static::Amiga::Target;
	auto *const target = new Target();
	target->media = media;
	targets.push_back(std::unique_ptr<Analyser::Static::Target>(target));

	return targets;
}
