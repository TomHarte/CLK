//
//  StaticAnalyser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/05/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"
#include "../AppleII/Target.hpp"

Analyser::Static::TargetList Analyser::Static::DiskII::GetTargets(const Media &media, const std::string &file_name, TargetPlatform::IntType potential_platforms) {
	using Target = Analyser::Static::AppleII::Target;
	auto target = std::unique_ptr<Target>(new Target);
	target->machine = Machine::AppleII;
	target->media = media;

	if(!target->media.disks.empty())
		target->disk_controller = Target::DiskController::SixteenSector;

	TargetList targets;
	targets.push_back(std::move(target));
	return targets;
}
