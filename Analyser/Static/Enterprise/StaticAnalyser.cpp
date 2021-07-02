//
//  StaticAnalyser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 24/06/2021.
//  Copyright 2021 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"
#include "Target.hpp"

#include "../../../Storage/Disk/Parsers/FAT.hpp"

Analyser::Static::TargetList Analyser::Static::Enterprise::GetTargets(const Media &media, const std::string &, TargetPlatform::IntType) {
	// This analyser can comprehend disks only.
	if(media.disks.empty()) return {};

	// Otherwise, assume a return will happen.
	Analyser::Static::TargetList targets;
	using Target = Analyser::Static::Enterprise::Target;
	auto *const target = new Target;
	target->media = media;

	// Always require a BASIC.
	target->basic_version = Target::BASICVersion::Any;

	// Inspect any supplied disks.
	if(!media.disks.empty()) {
		auto volume = Storage::Disk::FAT::GetVolume(media.disks.front());
		target->dos = Target::DOS::EXDOS;
	}

	targets.push_back(std::unique_ptr<Analyser::Static::Target>(target));

	return targets;
}
