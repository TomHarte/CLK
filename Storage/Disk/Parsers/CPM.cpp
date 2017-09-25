//
//  CPM.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/08/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "CPM.hpp"

#include "../Encodings/MFM/Parser.hpp"

using namespace Storage::Disk::CPM;

std::unique_ptr<Storage::Disk::CPM::Catalogue> Storage::Disk::CPM::GetCatalogue(const std::shared_ptr<Storage::Disk::Disk> &disk, const ParameterBlock &parameters) {
	Storage::Encodings::MFM::Parser parser(true, disk);

	// Assemble the actual bytes of the catalogue.
	std::vector<uint8_t> catalogue;
	size_t sector_size = 1;
	uint16_t catalogue_allocation_bitmap = parameters.catalogue_allocation_bitmap;
	if(!catalogue_allocation_bitmap) return nullptr;
	int sector = 0;
	int track = parameters.reserved_tracks;
	while(catalogue_allocation_bitmap) {
		if(catalogue_allocation_bitmap & 0x8000) {
			size_t size_read = 0;
			do {
				std::shared_ptr<Storage::Encodings::MFM::Sector> sector_contents = parser.get_sector(0, (uint8_t)track, (uint8_t)(parameters.first_sector + sector));
				if(!sector_contents) {
					return nullptr;
				}

				catalogue.insert(catalogue.end(), sector_contents->data.begin(), sector_contents->data.end());
				sector_size = sector_contents->data.size();

				size_read += sector_size;
				sector++;
				if(sector == parameters.sectors_per_track) {
					sector = 0;
					track++;
				}
			} while(size_read < (size_t)parameters.block_size);
		}

		catalogue_allocation_bitmap <<= 1;
	}

	std::unique_ptr<Catalogue> result(new Catalogue);
	bool has_long_allocation_units = (parameters.tracks * parameters.sectors_per_track * (int)sector_size / parameters.block_size) >= 256;
	size_t bytes_per_catalogue_entry = (has_long_allocation_units ? 16 : 8) * (size_t)parameters.block_size;

	// From the catalogue, create files.
	std::map<std::vector<uint8_t>, size_t> indices_by_name;
	File empty_file;
	for(size_t c = 0; c < catalogue.size(); c += 32) {
		// Skip this file if it's deleted; this is marked by it having 0xe5 as its user number
		if(catalogue[c] == 0xe5) continue;

		// Check whether this file has yet been seen; if not then add it to the list
		std::vector<uint8_t> descriptor;
		size_t index;
		descriptor.insert(descriptor.begin(), &catalogue[c], &catalogue[c + 12]);
		auto iterator = indices_by_name.find(descriptor);
		if(iterator != indices_by_name.end()) {
			index = iterator->second;
		} else {
			File new_file;
			new_file.user_number = catalogue[c];
			for(size_t s = 0; s < 8; s++) new_file.name.push_back((char)catalogue[c + s + 1]);
			for(size_t s = 0; s < 3; s++) new_file.type.push_back((char)catalogue[c + s + 9] & 0x7f);
			new_file.read_only = catalogue[c + 9] & 0x80;
			new_file.system = catalogue[c + 10] & 0x80;
			index = result->files.size();
			result->files.push_back(new_file);
			indices_by_name[descriptor] = index;
		}

		// figure out where this data needs to be pasted in
		size_t extent = (size_t)(catalogue[c + 12] + (catalogue[c + 14] << 5));
		int number_of_records = catalogue[c + 15];

		size_t required_size = extent * bytes_per_catalogue_entry + (size_t)number_of_records * 128;
		if(result->files[index].data.size() < required_size) {
			result->files[index].data.resize(required_size);
		}

		int sectors_per_block = parameters.block_size / (int)sector_size;
		int records_per_sector = (int)sector_size / 128;
		int record = 0;
		for(size_t block = 0; block < (has_long_allocation_units ? 8 : 16) && record < number_of_records; block++) {
			int block_number;
			if(has_long_allocation_units) {
				block_number = catalogue[c + 16 + (block << 1)] + (catalogue[c + 16 + (block << 1) + 1] << 8);
			} else {
				block_number = catalogue[c + 16 + block];
			}
			if(!block_number) {
				record += parameters.block_size / 128;
				continue;
			}
			int first_sector = block_number * sectors_per_block;

			sector = first_sector % parameters.sectors_per_track;
			track = first_sector / parameters.sectors_per_track;

			for(int s = 0; s < sectors_per_block && record < number_of_records; s++) {
				std::shared_ptr<Storage::Encodings::MFM::Sector> sector_contents = parser.get_sector(0, (uint8_t)track, (uint8_t)(parameters.first_sector +  sector));
				if(!sector_contents) break;
				sector++;
				if(sector == parameters.sectors_per_track) {
					sector = 0;
					track++;
				}

				int records_to_copy = std::min(number_of_records - record, records_per_sector);
				memcpy(&result->files[index].data[extent * bytes_per_catalogue_entry + (size_t)record * 128], sector_contents->data.data(), (size_t)records_to_copy * 128);
				record += records_to_copy;
			}
		}
	}

	return result;
}
