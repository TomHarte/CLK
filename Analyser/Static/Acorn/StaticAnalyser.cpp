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
	auto targetElectron = std::make_unique<ElectronTarget>();
	auto targetBBC = std::make_unique<BBCMicroTarget>();
	auto targetArchimedes = std::make_unique<ArchimedesTarget>();
	int bbc_hits = 0;
	int electron_hits = 0;

	// Copy appropriate cartridges to the 8-bit targets.
	targetElectron->media.cartridges = AcornCartridgesFrom(media.cartridges);
	targetBBC->media.cartridges = AcornCartridgesFrom(media.cartridges);

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
			targetElectron->loading_command = is_basic ? "CHAIN\"\"\n" : "*RUN\n";
			targetElectron->media.tapes = media.tapes;

			// TODO: my BBC Micro doesn't yet support tapes; evaluate here in the future.
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

			// Electron: use the Pres ADFS if using an ADFS, as it leaves Page at &EOO.
			targetElectron->media.disks = media.disks;
			targetElectron->has_dfs = bool(dfs_catalogue);
			targetElectron->has_pres_adfs = bool(adfs_catalogue);

			// BBC: only the 1770 DFS is currently supported, so use that.
			targetBBC->media.disks = media.disks;
			targetBBC->has_1770dfs = bool(dfs_catalogue);
			targetBBC->has_adfs = bool(adfs_catalogue);

			// Check whether a simple shift+break will do for loading this disk.
			const auto bootOption = (dfs_catalogue ?: adfs_catalogue)->bootOption;
			if(bootOption != Catalogue::BootOption::None) {
				targetBBC->should_shift_restart = targetElectron->should_shift_restart = true;
			} else {
				targetBBC->loading_command = targetElectron->loading_command = "*CAT\n";
			}

			for(const auto &file: dfs_catalogue ? dfs_catalogue->files : adfs_catalogue->files) {
				// Electron: check whether adding the AP6 ROM is justified.
				// For now this is an incredibly dense text search;
				// if any of the commands that aren't usually present
				// on a stock Electron are here, add the AP6 ROM and
				// some sideways RAM such that the SR commands are useful.
				for(const auto &command: {
					"AQRPAGE", "BUILD", "DUMP", "FORMAT", "INSERT", "LANG", "LIST", "LOADROM",
					"LOCK", "LROMS", "RLOAD", "ROMS", "RSAVE", "SAVEROM", "SRLOAD", "SRPAGE",
					"SRUNLOCK", "SRWIPE", "TUBE", "TYPE", "UNLOCK", "UNPLUG", "UROMS",
					"VERIFY", "ZERO"
				}) {
					if(std::search(file.data.begin(), file.data.end(), command, command+strlen(command)) != file.data.end()) {
						targetElectron->has_ap6_rom = true;
						targetElectron->has_sideways_ram = true;
					}
				}

				// Look for any 'BBC indicators', i.e. direct access to BBC-specific hardware.
				// Also currently a dense search.

				const auto hits = [&](const std::initializer_list<uint16_t> collection) {
					int hits = 0;
					for(const auto address: collection) {
						const uint8_t sta_address[3] = {
							0x8d, uint8_t(address & 0xff), uint8_t(address >> 8)
						};

						if(std::search(
							file.data.begin(), file.data.end(),
							std::begin(sta_address), std::end(sta_address)
						) != file.data.end()) {
							++hits;
						}
					}
					return hits;
				};

				bbc_hits += hits({
					// The video control registers.
					0xfe20, 0xfe21,

					// The system VIA.
					0xfe40, 0xfe41, 0xfe42, 0xfe43, 0xfe44, 0xfe45, 0xfe46, 0xfe47,
					0xfe48, 0xfe49, 0xfe4a, 0xfe4b, 0xfe4c, 0xfe4d, 0xfe4e, 0xfe4f,

					// The user VIA.
					0xfe60, 0xfe61, 0xfe62, 0xfe63, 0xfe64, 0xfe65, 0xfe66, 0xfe67,
					0xfe68, 0xfe69, 0xfe6a, 0xfe6b, 0xfe6c, 0xfe6d, 0xfe6e, 0xfe6f,
				});
				// BASIC for "MODE7".
				constexpr uint8_t mode7[] = {0xeb, 0x37};
				bbc_hits += std::search(
							file.data.begin(), file.data.end(),
							std::begin(mode7), std::end(mode7)
						) != file.data.end();

				electron_hits += hits({
					// ULA addresses that aren't also the BBC's CRTC.
					0xfe03, 0xfe04, 0xfe05,
					0xfe06, 0xfe07, 0xfe08,
					0xfe09, 0xfe0a, 0xfe0b,
					0xfe0c, 0xfe0d, 0xfe0e,
					0xfe0f,
				});
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
				static constexpr char read[] = "read";
				static constexpr char boot[] = "boot";
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
		targetElectron->has_pres_adfs = false;	// To override a floppy selection, if one was made.
		targetElectron->has_acorn_adfs = true;

		// Assume some sort of later-era Acorn work is likely to happen;
		// so ensure *TYPE, etc are present.
		targetElectron->has_ap6_rom = true;
		targetElectron->has_sideways_ram = true;

		targetElectron->media.mass_storage_devices = media.mass_storage_devices;

		// Check for a boot option.
		const auto sector = targetElectron->media.mass_storage_devices.front()->get_block(1);
		if(sector[0xfd]) {
			targetElectron->should_shift_restart = true;
		} else {
			targetElectron->loading_command = "*CAT\n";
		}
	}

	TargetList targets;
	if(!targetElectron->media.empty() && !targetBBC->media.empty()) {
		if(bbc_hits > electron_hits) {
			targets.push_back(std::move(targetBBC));
		} else {
			targets.push_back(std::move(targetElectron));
		}
	} else {
		if(!targetElectron->media.empty()) {
			targets.push_back(std::move(targetElectron));
		} else if(!targetBBC->media.empty()) {
			targets.push_back(std::move(targetBBC));
		}
	}
	if(!targetArchimedes->media.empty()) {
		targets.push_back(std::move(targetArchimedes));
	}
	return targets;
}
