//
//  AmstradCPC.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 30/07/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"
#include "../../Storage/Disk/Parsers/CPM.hpp"

static void InspectDataCatalogue(
	const std::unique_ptr<Storage::Disk::CPM::Catalogue> &data_catalogue,
	StaticAnalyser::Target &target) {
	// If there's just one file, run that.
	if(data_catalogue->files.size() == 1) {
		target.loadingCommand = "run\"" + data_catalogue->files[0].name + "\n";
		return;
	}

	// If only one file is [potentially] BASIC, run that one.
	int basic_files = 0;
	size_t last_basic_file = 0;
	for(size_t c = 0; c < data_catalogue->files.size(); c++) {
		if(!((data_catalogue->files[c].data[18] >> 1) & 7)) {
			basic_files++;
			last_basic_file = c;
		}
	}
	if(basic_files == 1) {
		target.loadingCommand = "run\"" + data_catalogue->files[last_basic_file].name + "\n";
		return;
	}

	// Desperation.
	target.loadingCommand = "cat\n";
}

static void InspectSystemCatalogue(
	const std::unique_ptr<Storage::Disk::CPM::Catalogue> &data_catalogue,
	StaticAnalyser::Target &target) {
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
		target.loadingCommand = "|tape\nrun\"\n";
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
			data_format.sectors_per_track = 9;
			data_format.tracks = 40;
			data_format.block_size = 1024;
			data_format.first_sector = 0x41;
			data_format.catalogue_allocation_bitmap = 0xc000;
			data_format.reserved_tracks = 2;

			std::unique_ptr<Storage::Disk::CPM::Catalogue> system_catalogue = Storage::Disk::CPM::GetCatalogue(target.disks.front(), system_format);
			if(system_catalogue) {
				InspectSystemCatalogue(data_catalogue, target);
			}
		}
	}

	destination.push_back(target);
}
