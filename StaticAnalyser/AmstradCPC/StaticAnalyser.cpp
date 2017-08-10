//
//  AmstradCPC.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 30/07/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"
#include "../../Storage/Disk/Parsers/CPM.hpp"

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

	target.amstradcpc.model = target.disks.empty() ? AmstradCPCModel::CPC464 : AmstradCPCModel::CPC6128;

	if(!target.disks.empty()) {
		// This is CPC data format.
		Storage::Disk::CPM::ParameterBlock parameters;
		parameters.sectors_per_track = 9;
		parameters.sector_size = 512;
		parameters.first_sector = 0xc1;
		parameters.catalogue_allocation_bitmap = 0xc000;
		parameters.reserved_tracks = 0;

		Storage::Disk::CPM::GetCatalogue(target.disks.front(), parameters);
	}

	destination.push_back(target);
}
