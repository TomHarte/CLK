//
//  AmstradCPC.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 30/07/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"

#include "../../Storage/Disk/Parsers/CPM.hpp"
#include "../../Storage/Disk/Encodings/MFM.hpp"

static bool strcmp_insensitive(const char *a, const char *b) {
	if(strlen(a) != strlen(b)) return false;
	while(*a) {
		if(tolower(*a) != towlower(*b)) return false;
		a++;
		b++;
	}
	return true;
}

static bool is_implied_extension(const std::string &extension) {
	return
		extension == "   " ||
		strcmp_insensitive(extension.c_str(), "BAS") ||
		strcmp_insensitive(extension.c_str(), "BIN");
}

static std::string RunCommandFor(const Storage::Disk::CPM::File &file) {
	// Trim spaces from the name.
	std::string name = file.name;
	name.erase(std::find_if(name.rbegin(), name.rend(), [](int ch) {
        return !std::isspace(ch);
    }).base(), name.end());

	// Form the basic command.
	std::string command = "run\"" + name;

	// Consider whether the extension is required.
	if(!is_implied_extension(file.type)) {
		command += "." + file.type;
	}

	// Add a newline and return.
	return command + "\n";
}

static void InspectDataCatalogue(
	const Storage::Disk::CPM::Catalogue &catalogue,
	StaticAnalyser::Target &target) {
	// Make a copy of all files and filter out any that are marked as system.
	std::vector<Storage::Disk::CPM::File> candidate_files = catalogue.files;
	candidate_files.erase(
		std::remove_if(candidate_files.begin(), candidate_files.end(), [](const Storage::Disk::CPM::File &file) {
			return file.system;
		}),
		candidate_files.end());

	// If there's just one file, run that.
	if(candidate_files.size() == 1) {
		target.loadingCommand = RunCommandFor(candidate_files[0]);
		return;
	}

	// If only one file is [potentially] BASIC, run that one; otherwise if only one has a suffix
	// that AMSDOS allows to be omitted, pick that one.
	int basic_files = 0;
	int implicit_suffixed_files = 0;

	size_t last_basic_file = 0;
	size_t last_implicit_suffixed_file = 0;

	for(size_t c = 0; c < candidate_files.size(); c++) {
		// Files with nothing but spaces in their name can't be loaded by the user, so disregard them.
		if(candidate_files[c].type == "   " && candidate_files[c].name == "        ")
			continue;

		// Check for whether this is [potentially] BASIC.
		if(candidate_files[c].data.size() >= 128 && !((candidate_files[c].data[18] >> 1) & 7)) {
			basic_files++;
			last_basic_file = c;
		}

		// Check suffix for emptiness.
		if(is_implied_extension(candidate_files[c].type)) {
			implicit_suffixed_files++;
			last_implicit_suffixed_file = c;
		}
	}
	if(basic_files == 1 || implicit_suffixed_files == 1) {
		size_t selected_file = (basic_files == 1) ? last_basic_file : last_implicit_suffixed_file;
		target.loadingCommand = RunCommandFor(candidate_files[selected_file]);
		return;
	}

	// Desperation.
	target.loadingCommand = "cat\n";
}

static void InspectSystemCatalogue(
	const std::shared_ptr<Storage::Disk::Disk> &disk,
	const Storage::Disk::CPM::Catalogue &catalogue,
	StaticAnalyser::Target &target) {
	Storage::Encodings::MFM::Parser parser(true, disk);

	// Check that the boot sector exists and looks like it had content written to it.
	std::shared_ptr<Storage::Encodings::MFM::Sector> boot_sector = parser.get_sector(0, 0, 0x41);
	if(boot_sector != nullptr) {
		// Check that the first 64 bytes of the sector aren't identical; if they are then probably
		// this disk was formatted and the filler byte never replaced.
		bool matched = true;
		for(size_t c = 1; c < 64; c++) {
			if(boot_sector->data[c] != boot_sector->data[0]) {
				matched = false;
				break;
			}
		}

		// This is a system disk, then launch it as though it were CP/M.
		if(!matched) {
			target.loadingCommand = "|cpm\n";
			return;
		}
	}

	InspectDataCatalogue(catalogue, target);
}

void StaticAnalyser::AmstradCPC::AddTargets(const Media &media, std::list<Target> &destination) {
	Target target;
	target.machine = Target::AmstradCPC;
	target.probability = 1.0;
	target.media.disks = media.disks;
	target.media.tapes = media.tapes;
	target.media.cartridges = media.cartridges;

	target.amstradcpc.model = AmstradCPCModel::CPC6128;

	if(!target.media.tapes.empty()) {
		// Ugliness flows here: assume the CPC isn't smart enough to pause between pressing
		// enter and responding to the follow-on prompt to press a key, so just type for
		// a while. Yuck!
		target.loadingCommand = "|tape\nrun\"\n1234567890";
	}

	if(!target.media.disks.empty()) {
		Storage::Disk::CPM::ParameterBlock data_format;
		data_format.sectors_per_track = 9;
		data_format.tracks = 40;
		data_format.block_size = 1024;
		data_format.first_sector = 0xc1;
		data_format.catalogue_allocation_bitmap = 0xc000;
		data_format.reserved_tracks = 0;

		std::unique_ptr<Storage::Disk::CPM::Catalogue> data_catalogue = Storage::Disk::CPM::GetCatalogue(target.media.disks.front(), data_format);
		if(data_catalogue) {
			InspectDataCatalogue(*data_catalogue, target);
		} else {
			Storage::Disk::CPM::ParameterBlock system_format;
			system_format.sectors_per_track = 9;
			system_format.tracks = 40;
			system_format.block_size = 1024;
			system_format.first_sector = 0x41;
			system_format.catalogue_allocation_bitmap = 0xc000;
			system_format.reserved_tracks = 2;

			std::unique_ptr<Storage::Disk::CPM::Catalogue> system_catalogue = Storage::Disk::CPM::GetCatalogue(target.media.disks.front(), system_format);
			if(system_catalogue) {
				InspectSystemCatalogue(target.media.disks.front(), *system_catalogue, target);
			}
		}
	}

	destination.push_back(target);
}
