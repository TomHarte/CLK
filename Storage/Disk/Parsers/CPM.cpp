//
//  CPM.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/08/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "CPM.hpp"

#include <algorithm>
#include <cstring>

#include "../Encodings/MFM/Parser.hpp"

using namespace Storage::Disk::CPM;

std::unique_ptr<Storage::Disk::CPM::Catalogue> Storage::Disk::CPM::GetCatalogue(const std::shared_ptr<Storage::Disk::Disk> &disk, const ParameterBlock &parameters) {
	Storage::Encodings::MFM::Parser parser(true, disk);

	// Assemble the actual bytes of the catalogue.
	std::vector<uint8_t> catalogue;
	std::size_t sector_size = 1;
	uint16_t catalogue_allocation_bitmap = parameters.catalogue_allocation_bitmap;
	if(!catalogue_allocation_bitmap) return nullptr;
	int sector = 0;
	int track = parameters.reserved_tracks;
	while(catalogue_allocation_bitmap) {
		if(catalogue_allocation_bitmap & 0x8000) {
			std::size_t size_read = 0;
			do {
				Storage::Encodings::MFM::Sector *sector_contents = parser.get_sector(0, uint8_t(track), uint8_t(parameters.first_sector + sector));
				if(!sector_contents || sector_contents->samples.empty()) {
					return nullptr;
				}

				catalogue.insert(catalogue.end(), sector_contents->samples[0].begin(), sector_contents->samples[0].end());
				sector_size = sector_contents->samples[0].size();

				size_read += sector_size;
				sector++;
				if(sector == parameters.sectors_per_track) {
					sector = 0;
					track++;
				}
			} while(size_read < size_t(parameters.block_size));
		}

		catalogue_allocation_bitmap <<= 1;
	}

	struct CatalogueEntry {
		uint8_t user_number;
		std::string name;
		std::string type;
		bool read_only;
		bool system;
		std::size_t extent;
		uint8_t number_of_records;
		std::size_t catalogue_index;

		bool operator < (const CatalogueEntry &rhs) const {
			return std::tie(user_number, name, type, extent) < std::tie(rhs.user_number, rhs.name, rhs.type, rhs.extent);
		}
		bool is_same_file(const CatalogueEntry &rhs) const {
			return std::tie(user_number, name, type) == std::tie(rhs.user_number, rhs.name, rhs.type);
		}
	};

	// From the catalogue, get catalogue entries.
	std::vector<CatalogueEntry> catalogue_entries;
	for(std::size_t c = 0; c < catalogue.size(); c += 32) {
		// Skip this file if it's deleted; this is marked by it having 0xe5 as its user number
		if(catalogue[c] == 0xe5) continue;

		catalogue_entries.emplace_back();
		CatalogueEntry &entry = catalogue_entries.back();
		entry.user_number = catalogue[c];
		entry.name.insert(entry.name.begin(), &catalogue[c+1], &catalogue[c+9]);
		for(std::size_t s = 0; s < 3; s++) entry.type.push_back(char(catalogue[c + s + 9]) & 0x7f);
		entry.read_only = catalogue[c + 9] & 0x80;
		entry.system = catalogue[c + 10] & 0x80;
		entry.extent = size_t(catalogue[c + 12] + (catalogue[c + 14] << 5));
		entry.number_of_records = catalogue[c + 15];
		entry.catalogue_index = c;
	}

	// Sort the catalogue entries and then map to files.
	std::sort(catalogue_entries.begin(), catalogue_entries.end());

	std::unique_ptr<Catalogue> result(new Catalogue);

	bool has_long_allocation_units = (parameters.tracks * parameters.sectors_per_track * int(sector_size) / parameters.block_size) >= 256;
	std::size_t bytes_per_catalogue_entry = (has_long_allocation_units ? 8 : 16) * size_t(parameters.block_size);
	int sectors_per_block = parameters.block_size / int(sector_size);
	int records_per_sector = int(sector_size) / 128;

	auto entry = catalogue_entries.begin();
	while(entry != catalogue_entries.end()) {
		// Find final catalogue entry that relates to the same file.
		auto final_entry = entry + 1;
		while(final_entry != catalogue_entries.end() && final_entry->is_same_file(*entry)) {
			final_entry++;
		}
		final_entry--;

		// Create file.
		result->files.emplace_back();
		File &new_file = result->files.back();
		new_file.user_number = entry->user_number;
		new_file.name = std::move(entry->name);
		new_file.type = std::move(entry->type);
		new_file.read_only = entry->read_only;
		new_file.system = entry->system;

		// Create storage for data.
		std::size_t required_size = final_entry->extent * bytes_per_catalogue_entry + size_t(final_entry->number_of_records) * 128;
		new_file.data.resize(required_size);

		// Accumulate all data.
		while(entry <= final_entry) {
			int record = 0;
			int number_of_records = (entry->number_of_records != 0x80) ? entry->number_of_records : (has_long_allocation_units ? 8 : 16);
			for(std::size_t block = 0; block < (has_long_allocation_units ? 8 : 16) && record < number_of_records; block++) {
				int block_number;
				if(has_long_allocation_units) {
					block_number = catalogue[entry->catalogue_index + 16 + (block << 1)] + (catalogue[entry->catalogue_index + 16 + (block << 1) + 1] << 8);
				} else {
					block_number = catalogue[entry->catalogue_index + 16 + block];
				}
				if(!block_number) {
					record += parameters.block_size / 128;
					continue;
				}
				int first_sector = block_number * sectors_per_block;

				sector = first_sector % parameters.sectors_per_track;
				track = first_sector / parameters.sectors_per_track;

				for(int s = 0; s < sectors_per_block && record < number_of_records; s++) {
					Storage::Encodings::MFM::Sector *sector_contents = parser.get_sector(0, uint8_t(track), uint8_t(parameters.first_sector +  sector));
					if(!sector_contents || sector_contents->samples.empty()) break;
					sector++;
					if(sector == parameters.sectors_per_track) {
						sector = 0;
						track++;
					}

					int records_to_copy = std::min(entry->number_of_records - record, records_per_sector);
					std::memcpy(&new_file.data[entry->extent * bytes_per_catalogue_entry + size_t(record) * 128], sector_contents->samples[0].data(), size_t(records_to_copy) * 128);
					record += records_to_copy;
				}
			}

			entry++;
		}
	}

	return result;
}
