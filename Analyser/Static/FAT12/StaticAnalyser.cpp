//
//  StaticAnalyser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/12/2023.
//  Copyright 2023 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"

#include "../Enterprise/StaticAnalyser.hpp"
#include "../PCCompatible/StaticAnalyser.hpp"

#include "../../../Storage/Disk/Track/TrackSerialiser.hpp"
#include "../../../Storage/Disk/Encodings/MFM/Constants.hpp"
#include "../../../Storage/Disk/Encodings/MFM/SegmentParser.hpp"


Analyser::Static::TargetList Analyser::Static::FAT12::GetTargets(const Media &media, const std::string &file_name, TargetPlatform::IntType platforms) {
	// This analyser can comprehend disks only.
	if(media.disks.empty()) return {};

	auto &disk = media.disks.front();
	TargetList targets;

	// Total list of potential platforms is:
	//
	//	* the Enterprise;
	//	* the Atari ST;
	//	* the MSX; and
	//	* the PC.
	//
	// (though the MSX and Atari ST don't currently call in here for now)

	// If the disk image is very small, map it to the PC. That's the only option old enough
	// to have used 5.25" media.
	if(disk->get_maximum_head_position() <= Storage::Disk::HeadPosition(40)) {
		return Analyser::Static::PCCompatible::GetTargets(media, file_name, platforms);
	}

	// Attempt to grab MFM track 0, sector 1: the boot sector.
	const auto track_zero = disk->get_track_at_position(Storage::Disk::Track::Address(0, Storage::Disk::HeadPosition(0)));
	const auto sector_map = Storage::Encodings::MFM::sectors_from_segment(
			Storage::Disk::track_serialisation(
				*track_zero,
				Storage::Encodings::MFM::MFMBitLength
			), true);

	// If no sectors were found, assume this disk was single density, which also implies the PC.
	if(sector_map.empty()) {
		return Analyser::Static::PCCompatible::GetTargets(media, file_name, platforms);
	}

	const Storage::Encodings::MFM::Sector *boot_sector = nullptr;
	for(const auto &pair: sector_map) {
		if(pair.second.address.sector == 1) {
			boot_sector = &pair.second;
			break;
		}
	}

	// This shouldn't technically be possible since the disk has been identified as FAT12, but be safe.
	if(!boot_sector) {
		return {};
	}

	// Check for key phrases that imply a PC disk.
	const auto &sample = boot_sector->samples[0];
	const std::vector<std::string> pc_strings = {
		"MSDOS",
		"Non-System disk or disk error",
		"Insert a SYSTEM disk",
	};
	for(const auto &string: pc_strings) {
		if(
			std::search(sample.begin(), sample.end(), string.begin(), string.end()) != sample.end()
		) {
			return Analyser::Static::PCCompatible::GetTargets(media, file_name, platforms);
		}
	}

	// TODO: attempt disassembly as 8086.

	// Being unable to prove that this is a PC disk, throw it to the Enterprise.
	return Analyser::Static::Enterprise::GetTargets(media, file_name, platforms);
}
