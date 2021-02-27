//
//  StaticAnalyser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 02/06/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"
#include "Target.hpp"

Analyser::Static::TargetList Analyser::Static::Macintosh::GetTargets(const Media &media, const std::string &, TargetPlatform::IntType) {
	// This analyser can comprehend disks and mass-storage devices only.
	if(media.disks.empty() && media.mass_storage_devices.empty()) return {};

	// As there is at least one usable media image, wave it through.
	Analyser::Static::TargetList targets;

	using Target = Analyser::Static::Macintosh::Target;
	auto *const target = new Target;
	target->media = media;

	// If this is a single-sided floppy disk, guess the Macintosh 512kb.
	if(media.mass_storage_devices.empty()) {
		bool has_800kb_disks = false;
		for(const auto &disk: media.disks) {
			has_800kb_disks |= disk->get_head_count() > 1;
		}

		if(!has_800kb_disks) {
			target->model = Target::Model::Mac512k;
		}
	}

	targets.push_back(std::unique_ptr<Analyser::Static::Target>(target));

	return targets;
}
