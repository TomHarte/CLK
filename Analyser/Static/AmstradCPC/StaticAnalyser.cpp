//
//  AmstradCPC.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 30/07/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"

#include "Storage/Disk/Parsers/CPM.hpp"
#include "Storage/Disk/Encodings/MFM/Parser.hpp"
#include "Storage/Tape/Parsers/Spectrum.hpp"

#include "Target.hpp"

#include <algorithm>
#include <cstring>
#include <optional>
#include <unordered_set>

namespace {

std::string rtrimmed(const std::string &input) {
	auto trimmed = input;
	trimmed.erase(std::find_if(trimmed.rbegin(), trimmed.rend(), [](const char ch) {
		return !std::isspace(ch);
	}).base(), trimmed.end());
	return trimmed;
}

bool strcmp_insensitive(const char *a, const char *b) {
	if(std::strlen(a) != std::strlen(b)) return false;
	while(*a) {
		if(std::tolower(*a) != std::tolower(*b)) return false;
		a++;
		b++;
	}
	return true;
}

bool is_implied_extension(const std::string &extension) {
	return
		extension == "   " ||
		strcmp_insensitive(extension.c_str(), "BAS") ||
		strcmp_insensitive(extension.c_str(), "BIN");
}

void right_trim(std::string &string) {
	string.erase(std::find_if(string.rbegin(), string.rend(), [](int ch) {
		return !std::isspace(ch);
	}).base(), string.end());
}

std::string RunCommandFor(const Storage::Disk::CPM::File &file) {
	// Trim spaces from the name.
	std::string name = file.name;
	right_trim(name);

	// Form the basic command.
	std::string command = "run\"" + name;

	// Consider whether the extension is required.
	if(!is_implied_extension(file.type)) {
		std::string type = file.type;
		right_trim(type);
		command += "." + type;
	}

	// Add a newline and return.
	return command + "\n";
}

void InspectCatalogue(
	const Storage::Disk::CPM::Catalogue &catalogue,
	const std::unique_ptr<Analyser::Static::AmstradCPC::Target> &target
) {
	std::vector<const Storage::Disk::CPM::File *> candidate_files;
	candidate_files.reserve(catalogue.files.size());
	for(const auto &file : catalogue.files) {
		candidate_files.push_back(&file);
	}

	// Remove all files with untypable characters.
	candidate_files.erase(
		std::remove_if(candidate_files.begin(), candidate_files.end(), [](const Storage::Disk::CPM::File *file) {
			for(const auto c : file->name + file->type) {
				if(c < 32) return true;
			}
			return false;
		}),
		candidate_files.end());

	// If that leaves a mix of 'system' (i.e. hidden) and non-system files, remove the system files.
	bool are_all_system = true;
	for(const auto &file : candidate_files) {
		if(!file->system) {
			are_all_system = false;
			break;
		}
	}

	if(!are_all_system) {
		candidate_files.erase(
			std::remove_if(candidate_files.begin(), candidate_files.end(), [](const Storage::Disk::CPM::File *file) {
				return file->system;
			}),
			candidate_files.end());
	}

	// If there's just one file, run that.
	if(candidate_files.size() == 1) {
		target->loading_command = RunCommandFor(*candidate_files[0]);
		return;
	}

	const auto run_name = [&]() -> std::optional<std::string> {
		// Collect:
		//
		//	1. a set of all files that can be run without specifying an extension plus their appearance counts;
		//	2. a set of all BASIC file names.
		std::unordered_map<std::string, int> candidates;
		std::unordered_set<std::string> basic_names;
		for(std::size_t c = 0; c < candidate_files.size(); c++) {
			// Files with nothing but spaces in their name can't be loaded by the user, so disregard them.
			if(
				(candidate_files[c]->type == "   " && candidate_files[c]->name == "        ") ||
				!is_implied_extension(candidate_files[c]->type)
			) {
				continue;
			}

			++candidates[candidate_files[c]->name];
			if(candidate_files[c]->data.size() >= 128 && !((candidate_files[c]->data[18] >> 1) & 7)) {
				basic_names.insert(candidate_files[c]->name);
			}
		}

		// Only one candidate total.
		if(candidates.size() == 1) {
			return candidates.begin()->first;
		}

		// Only one BASIC candidate.
		if(basic_names.size() == 1) {
			return *basic_names.begin();
		}

		// Exactly two candidate names, but only one is a unique name.
		if(candidates.size() == 2) {
			const auto item1 = candidates.begin();
			const auto item2 = std::next(item1);

			if(item1->second == 1 && item2->second != 1) {
				return item1->first;
			}
			if(item2->second == 1 && item1->second != 1) {
				return item2->first;
			}
		}

		// Remove from candidates anything that is just a suffixed version of
		// another name, as long as the other name is three or more characters.
		std::vector<std::string> to_remove;
		for(const auto &lhs: candidates) {
			const auto trimmed = rtrimmed(lhs.first);
			if(trimmed.size() < 3) {
				continue;
			}

			for(const auto &rhs: candidates) {
				if(lhs.first == rhs.first) {
					continue;
				}

				if(rhs.first.find(trimmed) == 0) {
					to_remove.push_back(rhs.first);
				}
			}
		}
		for(const auto &candidate: to_remove) {
			candidates.erase(candidate);
		}
		if(candidates.size() == 1) {
			return candidates.begin()->first;
		}

		return {};
	} ();

	if(run_name.has_value()) {
		target->loading_command = "run\"" + rtrimmed(*run_name) + "\n";
	} else {
		target->loading_command = "cat\n";
	}
}

bool CheckBootSector(
	const std::shared_ptr<Storage::Disk::Disk> &disk,
	const std::unique_ptr<Analyser::Static::AmstradCPC::Target> &target
) {
	Storage::Encodings::MFM::Parser parser(Storage::Encodings::MFM::Density::Double, disk);
	const Storage::Encodings::MFM::Sector *boot_sector = parser.sector(0, 0, 0x41);
	if(boot_sector != nullptr && !boot_sector->samples.empty() && boot_sector->samples[0].size() == 512) {
		// Check that the first 64 bytes of the sector aren't identical; if they are then probably
		// this disk was formatted and the filler byte never replaced.
		bool matched = true;
		for(std::size_t c = 1; c < 64; c++) {
			if(boot_sector->samples[0][c] != boot_sector->samples[0][0]) {
				matched = false;
				break;
			}
		}

		// This is a system disk, then launch it as though it were CP/M.
		if(!matched) {
			target->loading_command = "|cpm\n";
			return true;
		}
	}

	return false;
}

bool IsAmstradTape(Storage::Tape::TapeSerialiser &serialiser) {
	// Limited sophistication here; look for a CPC-style file header, that is
	// any Spectrum-esque block with a synchronisation character of 0x2c.
	//
	// More could be done here: parse the header, look for 0x16 data records.
	using Parser = Storage::Tape::ZXSpectrum::Parser;
	Parser parser(Parser::MachineType::AmstradCPC);

	while(true) {
		const auto block = parser.find_block(serialiser);
		if(!block) break;

		if(block->type == 0x2c) {
			return true;
		}
	}

	return false;
}

} // namespace

Analyser::Static::TargetList Analyser::Static::AmstradCPC::GetTargets(
	const Media &media,
	const std::string &,
	TargetPlatform::IntType,
	bool
) {
	TargetList destination;
	auto target = std::make_unique<Target>();
	target->confidence = 0.5;

	target->model = Target::Model::CPC6128;

	if(!media.tapes.empty()) {
		bool has_cpc_tape = false;
		for(auto &tape: media.tapes) {
			const auto serialiser = tape->serialiser();
			has_cpc_tape |= IsAmstradTape(*serialiser);
		}

		if(has_cpc_tape) {
			target->media.tapes = media.tapes;

			// Ugliness flows here: assume the CPC isn't smart enough to pause between pressing
			// enter and responding to the follow-on prompt to press a key, so just type for
			// a while. Yuck!
			target->loading_command = "|tape\nrun\"\n123";
		}
	}

	if(!media.disks.empty()) {
		const auto data_format = Storage::Disk::CPM::ParameterBlock::cpc_data_format();
		const auto system_format = Storage::Disk::CPM::ParameterBlock::cpc_system_format();

		for(auto &disk: media.disks) {
			// Check for an ordinary catalogue, making sure this isn't actually a ZX Spectrum disk.
			std::unique_ptr<Storage::Disk::CPM::Catalogue> data_catalogue =
				Storage::Disk::CPM::GetCatalogue(disk, data_format, false);
			if(data_catalogue && !data_catalogue->is_zx_spectrum_booter()) {
				InspectCatalogue(*data_catalogue, target);
				target->media.disks.push_back(disk);
				continue;
			}

			// Failing that check for a boot sector.
			if(CheckBootSector(disk, target)) {
				target->media.disks.push_back(disk);
				continue;
			}

			// Failing that check for a system catalogue.
			std::unique_ptr<Storage::Disk::CPM::Catalogue> system_catalogue =
				Storage::Disk::CPM::GetCatalogue(disk, system_format, false);
			if(system_catalogue && !system_catalogue->is_zx_spectrum_booter()) {
				InspectCatalogue(*system_catalogue, target);
				target->media.disks.push_back(disk);
				continue;
			}
		}
	}

	// If any media survived, add the target.
	if(!target->media.empty())
		destination.push_back(std::move(target));

	return destination;
}
