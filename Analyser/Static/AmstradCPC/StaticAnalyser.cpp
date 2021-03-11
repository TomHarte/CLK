//
//  AmstradCPC.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 30/07/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"

#include <algorithm>
#include <cstring>

#include "Target.hpp"

#include "../../../Storage/Disk/Parsers/CPM.hpp"
#include "../../../Storage/Disk/Encodings/MFM/Parser.hpp"
#include "../../../Storage/Tape/Parsers/Spectrum.hpp"

namespace {

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
	const std::unique_ptr<Analyser::Static::AmstradCPC::Target> &target) {

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

	// If only one file is [potentially] BASIC, run that one; otherwise if only one has a suffix
	// that AMSDOS allows to be omitted, pick that one.
	int basic_files = 0;
	int implicit_suffixed_files = 0;

	std::size_t last_basic_file = 0;
	std::size_t last_implicit_suffixed_file = 0;

	for(std::size_t c = 0; c < candidate_files.size(); c++) {
		// Files with nothing but spaces in their name can't be loaded by the user, so disregard them.
		if(candidate_files[c]->type == "   " && candidate_files[c]->name == "        ")
			continue;

		// Check for whether this is [potentially] BASIC.
		if(candidate_files[c]->data.size() >= 128 && !((candidate_files[c]->data[18] >> 1) & 7)) {
			basic_files++;
			last_basic_file = c;
		}

		// Check suffix for emptiness.
		if(is_implied_extension(candidate_files[c]->type)) {
			implicit_suffixed_files++;
			last_implicit_suffixed_file = c;
		}
	}
	if(basic_files == 1 || implicit_suffixed_files == 1) {
		std::size_t selected_file = (basic_files == 1) ? last_basic_file : last_implicit_suffixed_file;
		target->loading_command = RunCommandFor(*candidate_files[selected_file]);
		return;
	}

	// One more guess: if only one remaining candidate file has a different name than the others,
	// assume it is intended to stand out.
	std::map<std::string, int> name_counts;
	std::map<std::string, std::size_t> indices_by_name;
	std::size_t index = 0;
	for(const auto &file : candidate_files) {
		name_counts[file->name]++;
		indices_by_name[file->name] = index;
		index++;
	}
	if(name_counts.size() == 2) {
		for(const auto &pair : name_counts) {
			if(pair.second == 1) {
				target->loading_command = RunCommandFor(*candidate_files[indices_by_name[pair.first]]);
				return;
			}
		}
	}

	// Desperation.
	target->loading_command = "cat\n";
}

bool CheckBootSector(const std::shared_ptr<Storage::Disk::Disk> &disk, const std::unique_ptr<Analyser::Static::AmstradCPC::Target> &target) {
	Storage::Encodings::MFM::Parser parser(true, disk);
	Storage::Encodings::MFM::Sector *boot_sector = parser.get_sector(0, 0, 0x41);
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

bool IsAmstradTape(const std::shared_ptr<Storage::Tape::Tape> &tape) {
	// Limited sophistication here; look for a CPC-style file header, that is
	// any Spectrum-esque block with a synchronisation character of 0x2c.
	//
	// More could be done here: parse the header, look for 0x16 data records.
	using Parser = Storage::Tape::ZXSpectrum::Parser;
	Parser parser(Parser::MachineType::AmstradCPC);

	while(true) {
		const auto block = parser.find_block(tape);
		if(!block) break;

		if(block->type == 0x2c) {
			return true;
		}
	}

	return false;
}

} // namespace

Analyser::Static::TargetList Analyser::Static::AmstradCPC::GetTargets(const Media &media, const std::string &, TargetPlatform::IntType) {
	TargetList destination;
	auto target = std::make_unique<Target>();
	target->confidence = 0.5;

	target->model = Target::Model::CPC6128;

	if(!media.tapes.empty()) {
		bool has_cpc_tape = false;
		for(auto &tape: media.tapes) {
			has_cpc_tape |= IsAmstradTape(tape);
		}

		if(has_cpc_tape) {
			target->media.tapes = media.tapes;

			// Ugliness flows here: assume the CPC isn't smart enough to pause between pressing
			// enter and responding to the follow-on prompt to press a key, so just type for
			// a while. Yuck!
			target->loading_command = "|tape\nrun\"\n1234567890";
		}
	}

	if(!media.disks.empty()) {
		Storage::Disk::CPM::ParameterBlock data_format;
		data_format.sectors_per_track = 9;
		data_format.tracks = 40;
		data_format.block_size = 1024;
		data_format.first_sector = 0xc1;
		data_format.catalogue_allocation_bitmap = 0xc000;
		data_format.reserved_tracks = 0;

		Storage::Disk::CPM::ParameterBlock system_format;
		system_format.sectors_per_track = 9;
		system_format.tracks = 40;
		system_format.block_size = 1024;
		system_format.first_sector = 0x41;
		system_format.catalogue_allocation_bitmap = 0xc000;
		system_format.reserved_tracks = 2;

		for(auto &disk: media.disks) {
			// Check for an ordinary catalogue.
			std::unique_ptr<Storage::Disk::CPM::Catalogue> data_catalogue = Storage::Disk::CPM::GetCatalogue(disk, data_format);
			if(data_catalogue) {
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
			std::unique_ptr<Storage::Disk::CPM::Catalogue> system_catalogue = Storage::Disk::CPM::GetCatalogue(disk, system_format);
			if(system_catalogue) {
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
