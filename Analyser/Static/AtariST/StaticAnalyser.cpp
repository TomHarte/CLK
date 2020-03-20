//
//  StaticAnalyser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/10/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"
#include "Target.hpp"

Analyser::Static::TargetList Analyser::Static::AtariST::GetTargets(const Media &media, const std::string &file_name, TargetPlatform::IntType potential_platforms) {
	// This analyser can comprehend disks and mass-storage devices only.
	if(media.disks.empty()) return {};

	// As there is at least one usable media image, wave it through.
	Analyser::Static::TargetList targets;

	using Target = Analyser::Static::AtariST::Target;
	auto *target = new Target();
	target->media = media;
	targets.push_back(std::unique_ptr<Analyser::Static::Target>(target));

	return targets;
}
