//
//  CPM.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/08/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "CPM.hpp"

#include "../Encodings/MFM.hpp"

using namespace Storage::Disk::CPM;

std::unique_ptr<Storage::Disk::CPM::Catalogue> Storage::Disk::CPM::GetCatalogue(const std::shared_ptr<Storage::Disk::Disk> &disk, const ParameterBlock &parameters) {
	Storage::Encodings::MFM::Parser parser(true, disk);

	// Assemble the actual bytes of the catalogue.
	std::vector<uint8_t> catalogue;
	uint16_t catalogue_allocation_bitmap = parameters.catalogue_allocation_bitmap;
	int sector = 0;
	int track = parameters.reserved_tracks;
	while(catalogue_allocation_bitmap) {
		if(catalogue_allocation_bitmap & 0x8000) {
			std::shared_ptr<Storage::Encodings::MFM::Sector> sector_contents = parser.get_sector((uint8_t)track, (uint8_t)(parameters.first_sector + sector));
			if(!sector_contents) {
				return nullptr;
			}

			catalogue.insert(catalogue.end(), sector_contents->data.begin(), sector_contents->data.end());
		}

		catalogue_allocation_bitmap <<= 1;

		sector++;
		if(sector == parameters.sectors_per_track) {
			sector = 0;
			track++;
		}
	}

	return nullptr;
}
