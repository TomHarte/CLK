//
//  StaticAnalyser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/05/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"
#include "../AppleII/Target.hpp"

#include "../../../Storage/Disk/Track/TrackSerialiser.hpp"
#include "../../../Storage/Disk/Encodings/AppleGCR/SegmentParser.hpp"

Analyser::Static::TargetList Analyser::Static::DiskII::GetTargets(const Media &media, const std::string &file_name, TargetPlatform::IntType potential_platforms) {
	// This analyser can comprehend disks only.
	if(media.disks.empty()) return {};

	// Grab track 0, sector 0.
	auto track_zero = media.disks.front()->get_track_at_position(Storage::Disk::Track::Address(0, 0));
	auto sector_map = Storage::Encodings::AppleGCR::sectors_from_segment(
		Storage::Disk::track_serialisation(*track_zero, Storage::Time(1, 50000)));

	const Storage::Encodings::AppleGCR::Sector *sector_zero = nullptr;
	for(const auto &pair: sector_map) {
		if(!pair.second.address.sector) {
			sector_zero = &pair.second;
			break;
		}
	}
	
	using Target = Analyser::Static::AppleII::Target;
	auto target = std::unique_ptr<Target>(new Target);
	target->machine = Machine::AppleII;
	target->media = media;

	target->disk_controller = Target::DiskController::SixteenSector;

	TargetList targets;
	targets.push_back(std::move(target));
	return targets;
}
