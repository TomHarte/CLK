//
//  AmstradCPC.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 30/07/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"
#include "../../Storage/Disk/Parsers/CPM.hpp"

static bool strcmp_insensitive(const char *a, const char *b) {
	if(strlen(a) != strlen(b)) return false;
	while(*a) {
		if(tolower(*a) != towlower(*b)) return false;
		a++;
		b++;
	}
	return true;
}

static void InspectDataCatalogue(
	const std::unique_ptr<Storage::Disk::CPM::Catalogue> &data_catalogue,
	StaticAnalyser::Target &target) {
	// If there's just one file, run that.
	if(data_catalogue->files.size() == 1) {
		target.loadingCommand = "run\"" + data_catalogue->files[0].name + "\n";
		return;
	}

	// If only one file is [potentially] BASIC, run that one; otherwise if only one has a suffix
	// that AMSDOS allows to be omitted, pick that one.
	int basic_files = 0;
	int implicit_suffixed_files = 0;

	size_t last_basic_file = 0;
	size_t last_implicit_suffixed_file = 0;

	for(size_t c = 0; c < data_catalogue->files.size(); c++) {
		// Check for whether this is [potentially] BASIC.
		if(!((data_catalogue->files[c].data[18] >> 1) & 7)) {
			basic_files++;
			last_basic_file = c;
		}

		// Check suffix for emptiness.
		if(
			data_catalogue->files[c].type == "   " ||
			strcmp_insensitive(data_catalogue->files[c].type.c_str(), "BAS") ||
			strcmp_insensitive(data_catalogue->files[c].type.c_str(), "BIN")
		) {
			implicit_suffixed_files++;
			last_implicit_suffixed_file = c;
		}
	}
	if(basic_files == 1 || implicit_suffixed_files == 1) {
		size_t selected_file = (basic_files == 1) ? last_basic_file : last_implicit_suffixed_file;
		target.loadingCommand = "run\"" + data_catalogue->files[selected_file].name + "\n";
		return;
	}

	// Desperation.
	target.loadingCommand = "cat\n";
}

static void InspectSystemCatalogue(
	const std::unique_ptr<Storage::Disk::CPM::Catalogue> &data_catalogue,
	StaticAnalyser::Target &target) {
	// If this is a system disk, then launch it as though it were CP/M.
	target.loadingCommand = "|cpm\n";
}

void StaticAnalyser::AmstradCPC::AddTargets(
	const std::list<std::shared_ptr<Storage::Disk::Disk>> &disks,
	const std::list<std::shared_ptr<Storage::Tape::Tape>> &tapes,
	const std::list<std::shared_ptr<Storage::Cartridge::Cartridge>> &cartridges,
	std::list<StaticAnalyser::Target> &destination) {
	Target target;
	target.machine = Target::AmstradCPC;
	target.probability = 1.0;
	target.disks = disks;
	target.tapes = tapes;
	target.cartridges = cartridges;

	target.amstradcpc.model = AmstradCPCModel::CPC6128;

	if(!target.tapes.empty()) {
		// Ugliness flows here: assume the CPC isn't smart enough to pause between pressing
		// enter and responding to the follow-on prompt to press a key, so just type for
		// a while. Yuck!
		target.loadingCommand = "|tape\nrun\"\n1234567890";
	}

	if(!target.disks.empty()) {
		Storage::Disk::CPM::ParameterBlock data_format;
		data_format.sectors_per_track = 9;
		data_format.tracks = 40;
		data_format.block_size = 1024;
		data_format.first_sector = 0xc1;
		data_format.catalogue_allocation_bitmap = 0xc000;
		data_format.reserved_tracks = 0;

		std::unique_ptr<Storage::Disk::CPM::Catalogue> data_catalogue = Storage::Disk::CPM::GetCatalogue(target.disks.front(), data_format);
		if(data_catalogue) {
			InspectDataCatalogue(data_catalogue, target);
		} else {
			Storage::Disk::CPM::ParameterBlock system_format;
			system_format.sectors_per_track = 9;
			system_format.tracks = 40;
			system_format.block_size = 1024;
			system_format.first_sector = 0x41;
			system_format.catalogue_allocation_bitmap = 0xc000;
			system_format.reserved_tracks = 2;

			std::unique_ptr<Storage::Disk::CPM::Catalogue> system_catalogue = Storage::Disk::CPM::GetCatalogue(target.disks.front(), system_format);
			if(system_catalogue) {
				InspectSystemCatalogue(data_catalogue, target);
			}
		}
	}

	destination.push_back(target);
}
