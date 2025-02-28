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

#include "Numeric/StringSimilarity.hpp"

#include <algorithm>
#include <map>

using namespace Analyser::Static::Acorn;

static std::vector<std::shared_ptr<Storage::Cartridge::Cartridge>>
AcornCartridgesFrom(const std::vector<std::shared_ptr<Storage::Cartridge::Cartridge>> &cartridges) {
	std::vector<std::shared_ptr<Storage::Cartridge::Cartridge>> acorn_cartridges;

	for(const auto &cartridge : cartridges) {
		const auto &segments = cartridge->get_segments();

		// Only one mapped item is allowed.
		if(segments.size() != 1) continue;

		// Cartridges must be 8 or 16 kb in size.
		const Storage::Cartridge::Cartridge::Segment &segment = segments.front();
		if(segment.data.size() != 0x4000 && segment.data.size() != 0x2000) continue;

		// Check copyright string.
		const uint8_t copyright_offset = segment.data[7];
		if(
			segment.data[copyright_offset] != 0x00 ||
			segment.data[copyright_offset+1] != 0x28 ||
			segment.data[copyright_offset+2] != 0x43 ||
			segment.data[copyright_offset+3] != 0x29
		) continue;

		// Check language entry point.
		if(!(
			(segment.data[0] == 0x00 && segment.data[1] == 0x00 && segment.data[2] == 0x00) ||
			(segment.data[0] != 0x00 && segment.data[2] >= 0x80 && segment.data[2] < 0xc0)
			)) continue;

		// Check service entry point.
		if(!(segment.data[5] >= 0x80 && segment.data[5] < 0xc0)) continue;

		// Probability of a random binary blob that isn't an Acorn ROM proceeding to here:
		//		1/(2^32) *
		//		( ((2^24)-1)/(2^24)*(1/4)		+		1/(2^24)	) *
		//		1/4
		//	= something very improbable, around 1/16th of 1 in 2^32, but not exactly.
		acorn_cartridges.push_back(cartridge);
	}

	return acorn_cartridges;
}

Analyser::Static::TargetList Analyser::Static::Acorn::GetTargets(
	const Media &media,
	const std::string &file_name,
	TargetPlatform::IntType,
	bool
) {
	auto target8bit = std::make_unique<ElectronTarget>();
	auto targetArchimedes = std::make_unique<ArchimedesTarget>();

	// Copy appropriate cartridges to the 8-bit target.
	target8bit->media.cartridges = AcornCartridgesFrom(media.cartridges);

	// If there are tapes, attempt to get data from the first.
	if(!media.tapes.empty()) {
		std::shared_ptr<Storage::Tape::Tape> tape = media.tapes.front();
		auto serialiser = tape->serialiser();
		std::vector<File> files = GetFiles(*serialiser);

		// continue if there are any files
		if(!files.empty()) {
			bool is_basic = true;

			// If a file is execute-only, that means *RUN.
			if(files.front().flags & File::Flags::ExecuteOnly) {
				is_basic = false;
			}

			// Check also for a continuous threading of BASIC lines; if none then this probably isn't BASIC code,
			// so that's also justification to *RUN.
			if(is_basic) {
				std::size_t pointer = 0;
				uint8_t *const data = &files.front().data[0];
				const std::size_t data_size = files.front().data.size();
				while(true) {
					if(pointer >= data_size-1 || data[pointer] != 0x0d) {
						is_basic = false;
						break;
					}
					if((data[pointer+1]&0x7f) == 0x7f) break;
					pointer += data[pointer+3];
				}
			}

			// Inspect first file. If it's protected or doesn't look like BASIC
			// then the loading command is *RUN. Otherwise it's CHAIN"".
			target8bit->loading_command = is_basic ? "CHAIN\"\"\n" : "*RUN\n";
			target8bit->media.tapes = media.tapes;
		}
	}

	if(!media.disks.empty()) {
		std::shared_ptr<Storage::Disk::Disk> disk = media.disks.front();
		std::unique_ptr<Catalogue> dfs_catalogue, adfs_catalogue;

		// Get any sort of catalogue that can be found.
		dfs_catalogue = GetDFSCatalogue(disk);
		if(dfs_catalogue == nullptr) adfs_catalogue = GetADFSCatalogue(disk);

		// 8-bit options: DFS and Hugo-style ADFS.
		if(dfs_catalogue || (adfs_catalogue && !adfs_catalogue->has_large_sectors && adfs_catalogue->is_hugo)) {
			// Accept the disk and determine whether DFS or ADFS ROMs are implied.
			// Use the Pres ADFS if using an ADFS, as it leaves Page at &EOO.
			target8bit->media.disks = media.disks;
			target8bit->has_dfs = bool(dfs_catalogue);
			target8bit->has_pres_adfs = bool(adfs_catalogue);

			// Check whether a simple shift+break will do for loading this disk.
			const auto bootOption = (dfs_catalogue ?: adfs_catalogue)->bootOption;
			if(bootOption != Catalogue::BootOption::None) {
				target8bit->should_shift_restart = true;
			} else {
				target8bit->loading_command = "*CAT\n";
			}

			// Check whether adding the AP6 ROM is justified.
			// For now this is an incredibly dense text search;
			// if any of the commands that aren't usually present
			// on a stock Electron are here, add the AP6 ROM and
			// some sideways RAM such that the SR commands are useful.
			for(const auto &file: dfs_catalogue ? dfs_catalogue->files : adfs_catalogue->files) {
				for(const auto &command: {
					"AQRPAGE", "BUILD", "DUMP", "FORMAT", "INSERT", "LANG", "LIST", "LOADROM",
					"LOCK", "LROMS", "RLOAD", "ROMS", "RSAVE", "SAVEROM", "SRLOAD", "SRPAGE",
					"SRUNLOCK", "SRWIPE", "TUBE", "TYPE", "UNLOCK", "UNPLUG", "UROMS",
					"VERIFY", "ZERO"
				}) {
					if(std::search(file.data.begin(), file.data.end(), command, command+strlen(command)) != file.data.end()) {
						target8bit->has_ap6_rom = true;
						target8bit->has_sideways_ram = true;
					}
				}
			}
		} else if(adfs_catalogue) {
			// Archimedes options, implicitly: ADFS, non-Hugo.
			targetArchimedes->media.disks = media.disks;

			// Also look for the best possible startup program name, if it can be discerned.
			std::multimap<double, std::string, std::greater<double>> options;
			for(const auto &file: adfs_catalogue->files) {
				// Skip non-Pling files.
				if(file.name[0] != '!') continue;

				// Take whatever else comes with a preference for things that don't
				// have 'boot' or 'read' in them (the latter of which will tend to be
				// read_me or read_this or similar).
				constexpr char read[] = "read";
				constexpr char boot[] = "boot";
				const auto has = [&](const char *begin, const char *end) {
					return  std::search(
						file.name.begin(), file.name.end(),
						begin, end - 1, // i.e. don't compare the trailing NULL.
						[](char lhs, char rhs) {
							return std::tolower(lhs) == rhs;
						}
					) != file.name.end();
				};
				const auto has_read = has(std::begin(read), std::end(read));
				const auto has_boot = has(std::begin(boot), std::end(boot));

				const auto probability =
					Numeric::similarity(file.name, adfs_catalogue->name) +
					Numeric::similarity(file.name, file_name) -
					((has_read || has_boot) ? 0.2 : 0.0);
				options.emplace(probability, file.name);
			}

			if(!options.empty()) {
				targetArchimedes->main_program = options.begin()->second;
			}
		}
	}

	// Enable the Acorn ADFS if a mass-storage device is attached;
	// unlike the Pres ADFS it retains SCSI logic.
	if(!media.mass_storage_devices.empty()) {
		target8bit->has_pres_adfs = false;	// To override a floppy selection, if one was made.
		target8bit->has_acorn_adfs = true;

		// Assume some sort of later-era Acorn work is likely to happen;
		// so ensure *TYPE, etc are present.
		target8bit->has_ap6_rom = true;
		target8bit->has_sideways_ram = true;

		target8bit->media.mass_storage_devices = media.mass_storage_devices;

		// Check for a boot option.
		const auto sector = target8bit->media.mass_storage_devices.front()->get_block(1);
		if(sector[0xfd]) {
			target8bit->should_shift_restart = true;
		} else {
			target8bit->loading_command = "*CAT\n";
		}
	}

	TargetList targets;
	if(!target8bit->media.empty()) {
		targets.push_back(std::move(target8bit));
	}
	if(!targetArchimedes->media.empty()) {
		targets.push_back(std::move(targetArchimedes));
	}
	return targets;
}
