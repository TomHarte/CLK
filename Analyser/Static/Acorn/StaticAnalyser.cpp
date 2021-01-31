//
//  AcornAnalyser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/08/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"

#include "Disk.hpp"
#include "Tape.hpp"
#include "Target.hpp"

using namespace Analyser::Static::Acorn;

static std::vector<std::shared_ptr<Storage::Cartridge::Cartridge>>
		AcornCartridgesFrom(const std::vector<std::shared_ptr<Storage::Cartridge::Cartridge>> &cartridges) {
	std::vector<std::shared_ptr<Storage::Cartridge::Cartridge>> acorn_cartridges;

	for(const auto &cartridge : cartridges) {
		const auto &segments = cartridge->get_segments();

		// only one mapped item is allowed
		if(segments.size() != 1) continue;

		// which must be 8 or 16 kb in size
		const Storage::Cartridge::Cartridge::Segment &segment = segments.front();
		if(segment.data.size() != 0x4000 && segment.data.size() != 0x2000) continue;

		// is a copyright string present?
		const uint8_t copyright_offset = segment.data[7];
		if(
			segment.data[copyright_offset] != 0x00 ||
			segment.data[copyright_offset+1] != 0x28 ||
			segment.data[copyright_offset+2] != 0x43 ||
			segment.data[copyright_offset+3] != 0x29
		) continue;

		// is the language entry point valid?
		if(!(
			(segment.data[0] == 0x00 && segment.data[1] == 0x00 && segment.data[2] == 0x00) ||
			(segment.data[0] != 0x00 && segment.data[2] >= 0x80 && segment.data[2] < 0xc0)
			)) continue;

		// is the service entry point valid?
		if(!(segment.data[5] >= 0x80 && segment.data[5] < 0xc0)) continue;

		// probability of a random binary blob that isn't an Acorn ROM proceeding to here:
		//		1/(2^32) *
		//		( ((2^24)-1)/(2^24)*(1/4)		+		1/(2^24)	) *
		//		1/4
		//	= something very improbable, around 1/16th of 1 in 2^32, but not exactly.
		acorn_cartridges.push_back(cartridge);
	}

	return acorn_cartridges;
}

Analyser::Static::TargetList Analyser::Static::Acorn::GetTargets(const Media &media, const std::string &, TargetPlatform::IntType) {
	auto target = std::make_unique<Target>();
	target->confidence = 0.5; // TODO: a proper estimation
	target->has_dfs = false;
	target->has_adfs = false;
	target->should_shift_restart = false;

	// strip out inappropriate cartridges
	target->media.cartridges = AcornCartridgesFrom(media.cartridges);

	// if there are any tapes, attempt to get data from the first
	if(!media.tapes.empty()) {
		std::shared_ptr<Storage::Tape::Tape> tape = media.tapes.front();
		std::vector<File> files = GetFiles(tape);
		tape->reset();

		// continue if there are any files
		if(!files.empty()) {
			bool is_basic = true;

			// If a file is execute-only, that means *RUN.
			if(files.front().flags & File::Flags::ExecuteOnly) is_basic = false;

			// check also for a continuous threading of BASIC lines; if none then this probably isn't BASIC code,
			// so that's also justification to *RUN
			std::size_t pointer = 0;
			uint8_t *const data = &files.front().data[0];
			const std::size_t data_size = files.front().data.size();
			while(1) {
				if(pointer >= data_size-1 || data[pointer] != 13) {
					is_basic = false;
					break;
				}
				if((data[pointer+1]&0x7f) == 0x7f) break;
				pointer += data[pointer+3];
			}

			// Inspect first file. If it's protected or doesn't look like BASIC
			// then the loading command is *RUN. Otherwise it's CHAIN"".
			target->loading_command = is_basic ? "CHAIN\"\"\n" : "*RUN\n";

			target->media.tapes = media.tapes;
		}
	}

	if(!media.disks.empty()) {
		std::shared_ptr<Storage::Disk::Disk> disk = media.disks.front();
		std::unique_ptr<Catalogue> dfs_catalogue, adfs_catalogue;
		dfs_catalogue = GetDFSCatalogue(disk);
		if(dfs_catalogue == nullptr) adfs_catalogue = GetADFSCatalogue(disk);
		if(dfs_catalogue || adfs_catalogue) {
			target->media.disks = media.disks;
			target->has_dfs = !!dfs_catalogue;
			target->has_adfs = !!adfs_catalogue;

			Catalogue::BootOption bootOption = (dfs_catalogue ?: adfs_catalogue)->bootOption;
			if(bootOption != Catalogue::BootOption::None)
				target->should_shift_restart = true;
			else
				target->loading_command = "*CAT\n";
		}
	}

	TargetList targets;
	if(!target->media.empty()) {
		targets.push_back(std::move(target));
	}
	return targets;
}
