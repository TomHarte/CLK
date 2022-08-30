//
//  StaticAnalyser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/04/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"
#include "Target.hpp"

Analyser::Static::TargetList Analyser::Static::AppleII::GetTargets(const Media &media, const std::string &, TargetPlatform::IntType) {
	auto target = std::make_unique<Target>();
	target->media = media;

	// If any disks are present, attach a Disk II.
	if(!target->media.disks.empty()) {
		target->disk_controller = Target::DiskController::SixteenSector;
	}

	// The emulated SCSI card requires a IIe, so upgrade to that if
	// any mass storage is present.
	if(!target->media.mass_storage_devices.empty()) {
		target->model = Target::Model::EnhancedIIe;
	}

	TargetList targets;
	targets.push_back(std::move(target));
	return targets;
}
