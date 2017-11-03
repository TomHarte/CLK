//
//  CPCDSK.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/08/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "CPCDSK.hpp"

#include "../../Encodings/MFM/Constants.hpp"
#include "../../Encodings/MFM/Encoder.hpp"
#include "../../Encodings/MFM/SegmentParser.hpp"
#include "../../Track/TrackSerialiser.hpp"

#include <iostream>

using namespace Storage::Disk;

CPCDSK::CPCDSK(const char *file_name) :
	is_extended_(false) {
	FileHolder file(file_name);
	is_read_only_ = file.get_is_known_read_only();

	if(!file.check_signature("MV - CPC")) {
		is_extended_ = true;
		file.seek(0, SEEK_SET);
		if(!file.check_signature("EXTENDED"))
			throw ErrorNotCPCDSK;
	}

	// Don't really care about about the creator; skip.
	file.seek(0x30, SEEK_SET);
	head_position_count_ = file.get8();
	head_count_ = file.get8();

	// Used only for non-extended disks.
	long size_of_a_track = 0;

	// Used only for extended disks.
	std::vector<size_t> track_sizes;

	if(is_extended_) {
		// Skip two unused bytes and grab the track size table.
		file.seek(2, SEEK_CUR);
		for(int c = 0; c < head_position_count_ * head_count_; c++) {
			track_sizes.push_back(static_cast<size_t>(file.get8()) << 8);
		}
	} else {
		size_of_a_track = file.get16le();
	}

	long file_offset = 0x100;
	for(size_t c = 0; c < static_cast<size_t>(head_position_count_ * head_count_); c++) {
		if(!is_extended_ || (track_sizes[c] > 0)) {
			// Skip the introductory text, 'Track-Info\r\n' and its unused bytes.
			file.seek(file_offset + 16, SEEK_SET);

			tracks_.emplace_back(new Track);
			Track *track = tracks_.back().get();

			// Track and side are stored, being a byte each.
			track->track = file.get8();
			track->side = file.get8();
			
			// If this is an extended disk image then John Elliott's extension provides some greater
			// data rate and encoding context. Otherwise the next two bytes have no defined meaning.
			if(is_extended_) {
				switch(file.get8()) {
					default: track->data_rate = Track::DataRate::Unknown;				break;
					case 1: track->data_rate = Track::DataRate::SingleOrDoubleDensity;	break;
					case 2: track->data_rate = Track::DataRate::HighDensity;			break;
					case 3: track->data_rate = Track::DataRate::ExtendedDensity;		break;
				}
				switch(file.get8()) {
					default: track->data_encoding = Track::DataEncoding::Unknown;		break;
					case 1: track->data_encoding = Track::DataEncoding::FM;				break;
					case 2: track->data_encoding = Track::Track::DataEncoding::MFM;		break;
				}
			} else {
				track->data_rate = Track::DataRate::Unknown;
				track->data_encoding = Track::DataEncoding::Unknown;
				file.seek(2, SEEK_CUR);
			}
			
			// Sector size, number of sectors, gap 3 length and the filler byte are then common
			// between both variants of DSK.
			track->sector_length = file.get8();
			size_t number_of_sectors = file.get8();
			track->gap3_length = file.get8();
			track->filler_byte = file.get8();

			// Sector information begins immediately after the track information table.
			while(number_of_sectors--) {
				track->sectors.emplace_back();
				Track::Sector &sector = track->sectors.back();

				// Track, side, sector, size and two FDC8272-esque status bytes are stored
				// per sector, in both regular and extended DSK files.
				sector.address.track = file.get8();
				sector.address.side = file.get8();
				sector.address.sector = file.get8();
				sector.size = file.get8();
				sector.fdc_status1 = file.get8();
				sector.fdc_status2 = file.get8();

				if(sector.fdc_status2 & 0x20) {
					// The CRC failed in the data field.
					sector.has_data_crc_error = true;
				} else {
					if(sector.fdc_status1 & 0x20) {
						// The CRC failed in the ID field.
						sector.has_header_crc_error = true;
					}
				}

				if(sector.fdc_status2 & 0x40) {
					// This sector is marked as deleted.
					sector.is_deleted = true;
				}

				if(sector.fdc_status2 & 0x01) {
					// Data field wasn't found.
					sector.samples.clear();
				}

				// Figuring out the actual data size is a little more work...
				size_t data_size = static_cast<size_t>(128 << sector.size);
				size_t stored_data_size = data_size;
				size_t number_of_samplings = 1;

				if(is_extended_) {
					// In an extended DSK, oblige two Simon Owen extensions:
					//
					// Allow a declared data size less than the sector's declared size to act as an abbreviation.
					// Extended DSK varies the 8kb -> 0x1800 bytes special case by this means.
					//
					// Use a declared data size greater than the sector's declared size as a record that this
					// sector was weak or fuzzy and that multiple samplings are provided. If the greater size
					// is not an exact multiple then my reading of the documentation is that this is an invalid
					// disk image.
					size_t declared_data_size = file.get16le();
					if(declared_data_size != stored_data_size) {
						if(declared_data_size > data_size) {
							number_of_samplings = declared_data_size / data_size;
							if(declared_data_size % data_size)
								throw ErrorNotCPCDSK;
						} else {
							stored_data_size = declared_data_size;
						}
					}
				} else {
					// In a regular DSK, these two bytes are unused, and a special case is applied that ostensibly 8kb
					// sectors are abbreviated to only 0x1800 bytes.
					if(data_size == 0x2000) stored_data_size = 0x1800;
					file.seek(2, SEEK_CUR);
				}

				// As per the weak/fuzzy sector extension, multiple samplings may be stored here.
				// Plan to tead as many as there were.
				sector.samples.emplace_back();
				sector.samples.resize(number_of_samplings);
				while(number_of_samplings--) {
					sector.samples[number_of_samplings].resize(stored_data_size);
				}
			}

			// Sector contents are at offset 0x100 into the track.
			file.seek(file_offset + 0x100, SEEK_SET);
			for(auto &sector: track->sectors) {
				for(auto &data : sector.samples) {
					file.read(data.data(), data.size());
				}
			}
		} else {
			// An extended disk image, which declares that there is no data stored for this track.
			tracks_.emplace_back();
		}

		// Advance to the beginning of the next track.
		if(is_extended_)
			file_offset += static_cast<long>(track_sizes[c]);
		else
			file_offset += size_of_a_track;
	}
}

int CPCDSK::get_head_position_count() {
	return head_position_count_;
}

int CPCDSK::get_head_count() {
	return head_count_;
}

size_t CPCDSK::index_for_track(::Storage::Disk::Track::Address address) {
	return static_cast<size_t>((address.position * head_count_) + address.head);
}

std::shared_ptr<Track> CPCDSK::get_track_at_position(::Storage::Disk::Track::Address address) {
	// Given that thesea are interleaved images, determine which track, chronologically, is being requested.
	size_t chronological_track = index_for_track(address);

	// Return a nullptr if out of range or not provided.
	if(chronological_track >= tracks_.size()) return nullptr;
	
	Track *track = tracks_[chronological_track].get();
	if(!track) return nullptr;

	std::vector<const Storage::Encodings::MFM::Sector *> sectors;
	for(auto &sector : track->sectors) {
		sectors.push_back(&sector);
	}

	// TODO: FM encoding, data rate?
	return Storage::Encodings::MFM::GetMFMTrackWithSectors(sectors, track->gap3_length, track->filler_byte);
}

void CPCDSK::set_tracks(const std::map<::Storage::Disk::Track::Address, std::shared_ptr<::Storage::Disk::Track>> &tracks) {
	// Patch changed tracks into the disk image.
	for(auto &pair: tracks) {
		// Assume MFM for now; with extensions DSK can contain FM tracks.
		const bool is_double_density = true;
		std::map<size_t, Storage::Encodings::MFM::Sector> sectors =
			Storage::Encodings::MFM::sectors_from_segment(
				Storage::Disk::track_serialisation(*pair.second, is_double_density ? Storage::Encodings::MFM::MFMBitLength : Storage::Encodings::MFM::FMBitLength),
				is_double_density);

		// Find slot for track, making it if neccessary.
		size_t chronological_track = index_for_track(pair.first);
		if(chronological_track >= tracks_.size()) {
			tracks_.resize(chronological_track+1);
		}

		// Get the track, or create it if necessary.
		Track *track = tracks_[chronological_track].get();
		if(!track) {
			track = new Track;
			track->track = static_cast<uint8_t>(pair.first.position);
			track->side = static_cast<uint8_t>(pair.first.head);
			track->data_rate = Track::DataRate::SingleOrDoubleDensity;
			track->data_encoding = Track::DataEncoding::MFM;
			track->sector_length = 2;
			track->gap3_length = 78;
			track->filler_byte = 0xe5;

			tracks_[chronological_track] = std::unique_ptr<Track>(track);
		}

		// Store sectors.
		track->sectors.clear();
		for(auto &source_sector: sectors) {
			track->sectors.emplace_back();
			Track::Sector &sector = track->sectors.back();

			sector.address = source_sector.second.address;
			sector.size = source_sector.second.size;
			sector.has_data_crc_error = source_sector.second.has_data_crc_error;
			sector.has_header_crc_error = source_sector.second.has_header_crc_error;
			sector.is_deleted = source_sector.second.is_deleted;
			sector.samples = std::move(source_sector.second.samples);
		}
	}
	
	// Rewrite the entire disk image, in extended form.
}

bool CPCDSK::get_is_read_only() {
	return is_read_only_;
}
