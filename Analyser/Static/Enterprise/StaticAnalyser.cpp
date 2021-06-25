//
//  StaticAnalyser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 24/06/2021.
//  Copyright 2021 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"
#include "Target.hpp"

Analyser::Static::TargetList Analyser::Static::Enterprise::GetTargets(const Media &media, const std::string &, TargetPlatform::IntType) {
	// This analyser can comprehend disks only.
	if(media.disks.empty()) return {};

	// Otherwise, for now: wave it through.
	Analyser::Static::TargetList targets;

	using Target = Analyser::Static::Enterprise::Target;
	auto *const target = new Target;
	target->media = media;

	// Always require a BASIC.
	target->basic_version = Target::BASICVersion::Any;

	// If this is a single-sided floppy disk, guess the Macintosh 512kb.
	if(!media.disks.empty()) {
		target->dos = Target::DOS::EXDOS;
	}

	targets.push_back(std::unique_ptr<Analyser::Static::Target>(target));

	return targets;
}
