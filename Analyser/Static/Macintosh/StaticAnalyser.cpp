//
//  StaticAnalyser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 02/06/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"
#include "Target.hpp"

Analyser::Static::TargetList Analyser::Static::Macintosh::GetTargets(const Media &media, const std::string &file_name, TargetPlatform::IntType potential_platforms) {
	// This analyser can comprehend disks only.
	if(media.disks.empty()) return {};

	// If there is at least one disk, wave it through.
	Analyser::Static::TargetList targets;

	using Target = Analyser::Static::Macintosh::Target;
	auto *target = new Target;
	target->machine = Analyser::Machine::Macintosh;
	target->media = media;
	targets.push_back(std::unique_ptr<Analyser::Static::Target>(target));

	return targets;
}
