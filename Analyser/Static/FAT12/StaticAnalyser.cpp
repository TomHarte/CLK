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


Analyser::Static::TargetList Analyser::Static::FAT12::GetTargets(
	const Media &media,
	const std::string &file_name,
	TargetPlatform::IntType platforms,
	bool
) {
	// This analyser can comprehend disks only.
	if(media.disks.empty()) return {};

	auto &disk = media.disks.front();
	TargetList targets;

	// Total list of potential platforms is:
	//
	//	* the Enterprise (and, by extension, CP/M-targetted software);
	//	* the Atari ST;
	//	* the MSX (ditto on CP/M); and
	//	* the PC.
	//
	// (though the MSX and Atari ST don't currently call in here for now)

	// If the disk image is very small or large, map it to the PC. That's the only option old enough
	// to have used 5.25" media.
	if(disk->maximum_head_position() <= Storage::Disk::HeadPosition(40)) {
		return Analyser::Static::PCCompatible::GetTargets(media, file_name, platforms, true);
	}

	// Attempt to grab MFM track 0, sector 1: the boot sector.
	const auto track_zero =
		disk->track_at_position(Storage::Disk::Track::Address(0, Storage::Disk::HeadPosition(0)));
	const auto sector_map = Storage::Encodings::MFM::sectors_from_segment(
			Storage::Disk::track_serialisation(
				*track_zero,
				Storage::Encodings::MFM::MFMBitLength
			), Storage::Encodings::MFM::Density::Double);

	// If no sectors were found, assume this disk was either single density or high density, which both imply the PC.
	if(sector_map.empty() || sector_map.size() > 10) {
		return Analyser::Static::PCCompatible::GetTargets(media, file_name, platforms, true);
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
		// MS-DOS strings.
		"MSDOS",
		"Non-System disk or disk error",
		// DOS Plus strings.
		"Insert a SYSTEM disk",
	};
	for(const auto &string: pc_strings) {
		if(
			std::search(sample.begin(), sample.end(), string.begin(), string.end()) != sample.end()
		) {
			return Analyser::Static::PCCompatible::GetTargets(media, file_name, platforms, true);
		}
	}

	// TODO: look for a COM, EXE or BAT, inspect. AUTOEXEC.BAT and/or CONFIG.SYS could be either PC or MSX.
	// Disassembling the boot sector doesn't necessarily work, as several Enterprise titles out there in the wild seem
	// to have been created by WINIMAGE which adds an x86 PC-style boot sector.

	// Enterprise notes: EXOS files all start with a 16-byte header which should begin with a 0 and then have a type
	// byte that will be 0xa or lower; cf http://epbas.lgb.hu/readme.html
	//
	// Some disks commonly passed around as Enterprise software are actually CP/M software, expecting IS-DOS (the CP/M
	// clone) to be present. It's certainly possible the same could be true of MSX disks and MSX-DOS. So analysing COM
	// files probably means searching for CALL 5s and/or INT 21hs, if not a more rigorous disassembly.
	//
	// I have not been able to locate a copy of IS-DOS so there's probably not much that can be done here; perhaps I
	// could redirect to an MSX2 with MSX-DOS2? Though it'd be nicer if I had a machine that was pure CP/M.

	// Being unable to prove that this is a PC disk, throw it to the Enterprise.
	return Analyser::Static::Enterprise::GetTargets(media, file_name, platforms, false);
}
