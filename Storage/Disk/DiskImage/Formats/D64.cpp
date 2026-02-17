//
//  D64.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 01/08/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "D64.hpp"

#include <algorithm>
#include <cstring>
#include <set>
#include <sys/stat.h>

#include "Storage/Disk/Track/PCMTrack.hpp"
#include "Storage/Disk/Encodings/CommodoreGCR.hpp"
#include "Storage/Disk/Track/TrackSerialiser.hpp"
#include "Numeric/SizedInt.hpp"

using namespace Storage::Disk;

D64::D64(const std::string_view file_name) :
		file_(file_name) {
	// In D64, this is it for validation without imposing potential false-negative tests:
	// check that the file size appears to be correct. Stone-age stuff.
	if(file_.stats().st_size != 174848 && file_.stats().st_size != 196608)
		throw Error::InvalidFormat;

	number_of_tracks_ = (file_.stats().st_size == 174848) ? 35 : 40;

	// Then, ostensibly, this is a valid file. Pick a disk ID as a
	// function of the file_name, being the most stable thing available.
	for(const auto &character: file_name) {
		disk_id_ ^= character;
		disk_id_ = uint16_t((disk_id_ << 2) ^ (disk_id_ >> 13));
	}
}

HeadPosition D64::maximum_head_position() const {
	return HeadPosition(number_of_tracks_);
}

bool D64::is_read_only() const {
	return file_.is_known_read_only();
}

bool D64::represents(const std::string_view name) const {
	return name == file_.name();
}

D64::TrackExtent D64::track_extent(const Track::Address address) const {
	static constexpr int tracks_in_zone[] = {17, 7, 6, 10};
	static constexpr int sectors_by_zone[] = {21, 19, 18, 17};

	int offset_to_track = 0;
	int tracks_to_traverse = address.position.as_int();
	int zone = 0;
	for(int current_zone = 0; current_zone < 4; current_zone++) {
		const int tracks = std::min(tracks_to_traverse, tracks_in_zone[current_zone]);
		offset_to_track += tracks * sectors_by_zone[current_zone];
		tracks_to_traverse -= tracks;
		if(tracks == tracks_in_zone[current_zone]) {
			++zone;
		}
	}

	return TrackExtent {
		.file_offset = offset_to_track * 256,
		.zone = zone,
		.number_of_sectors = sectors_by_zone[zone]
	};
}

std::unique_ptr<Track> D64::track_at_position(const Track::Address address) const {
	// Seek to start of data.
	const auto extent = track_extent(address);
	std::lock_guard lock_guard(file_.file_access_mutex());
	file_.seek(extent.file_offset, Whence::SET);

	// Build up a PCM sampling of the GCR version of this track.

	// Format per sector:
	//
	// syncronisation: three $FFs directly in GCR
	// value $08 to announce a header
	// a checksum made of XORing the following four bytes
	// sector number (1 byte)
	// track number (1 byte)
	// disk ID (2 bytes)
	// five GCR bytes of value $55
	// = [6 bytes -> 7.5 GCR bytes] + ... = 21 GCR bytes
	//
	// syncronisation: three $FFs directly in GCR
	// value $07 to announce data
	// 256 data bytes
	// a checksum: the XOR of the previous 256 bytes
	// two bytes of vaue $00
	// = [260 bytes -> 325 GCR bytes] + 3 GCR bytes = 328 GCR bytes
	//
	// = 349 GCR bytes per sector

	std::size_t track_bytes = 349 * size_t(extent.number_of_sectors);
	std::vector<uint8_t> data(track_bytes);

	for(int sector = 0; sector < extent.number_of_sectors; sector++) {
		uint8_t *const sector_data = &data[size_t(sector) * 349];
		sector_data[0] = sector_data[1] = sector_data[2] = 0xff;

		const uint8_t sector_number = uint8_t(sector);							// Sectors count from 0.
		const uint8_t track_number = uint8_t(address.position.as_int() + 1);	// Tracks count from 1.
		uint8_t checksum = uint8_t(sector_number ^ track_number ^ disk_id_ ^ (disk_id_ >> 8));
		const uint8_t header_start[4] = {
			0x08, checksum, sector_number, track_number
		};
		Encodings::CommodoreGCR::encode_block(&sector_data[3], header_start);

		const uint8_t header_end[4] = {
			uint8_t(disk_id_ & 0xff), uint8_t(disk_id_ >> 8), 0, 0
		};
		Encodings::CommodoreGCR::encode_block(&sector_data[8], header_end);

		// Pad out post-header parts.
		static constexpr uint8_t zeros[4] = {0, 0, 0, 0};
		Encodings::CommodoreGCR::encode_block(&sector_data[13], zeros);
		sector_data[18] = 0x52;
		sector_data[19] = 0x94;
		sector_data[20] = 0xaf;

		// Get the actual contents.
		uint8_t source_data[256];
		file_.read(source_data, sizeof(source_data));

		// Compute the latest checksum.
		checksum = 0;
		for(int c = 0; c < 256; c++) {
			checksum ^= source_data[c];
		}

		// Put in another sync.
		sector_data[21] = sector_data[22] = sector_data[23] = 0xff;

		// Now start writing in the actual data.
		const uint8_t start_of_data[4] = {
			0x07, source_data[0], source_data[1], source_data[2]
		};
		Encodings::CommodoreGCR::encode_block(&sector_data[24], start_of_data);
		int source_data_offset = 3;
		int target_data_offset = 29;
		while((source_data_offset+4) < 256) {
			Encodings::CommodoreGCR::encode_block(&sector_data[target_data_offset], &source_data[source_data_offset]);
			target_data_offset += 5;
			source_data_offset += 4;
		}
		const uint8_t end_of_data[4] = {
			source_data[255], checksum, 0, 0
		};
		Encodings::CommodoreGCR::encode_block(&sector_data[target_data_offset], end_of_data);
	}

	return std::make_unique<PCMTrack>(PCMSegment(data));
}

void D64::set_tracks(const std::map<Track::Address, std::unique_ptr<Track>> &tracks) {
	for(const auto &[address, track]: tracks) {
		const auto extent = track_extent(address);
		std::map<int, std::vector<uint8_t>> decoded;

		// Get bit stream.
		const auto serialisation =
			Storage::Disk::track_serialisation(
				*track,
				Time(1, extent.number_of_sectors * 349 * 8)	// This is relative to a normalised world where
															// 1 unit of time = 1 track. So don't use
															// length_of_a_bit_in_time_zone, which is relative to
															// a wall clock.
			);

		// Decode sectors.
		Numeric::SizedInt<10> shifter = 0;
		int repeats = 2;
		auto bit = serialisation.data.begin();
		bool is_ended = false;
		const auto shift = [&] {
			shifter = uint16_t((shifter.get() << 1) | *bit);
			++bit;

			if(bit == serialisation.data.end()) {
				bit = serialisation.data.begin();
				--repeats;
				is_ended |= !repeats;
			}
		};
		const auto byte = [&] {
			for(int c = 0; c < 9; c++) {
				shift();
			}
			const auto result = Encodings::CommodoreGCR::decoding_from_dectet(shifter.get());
			shift();
			return uint8_t(result);
		};
		const auto block_type = [&] {
			// Find synchronisation, then get first dectet after that.
			while(!is_ended && shifter.get() != 0b11111'11111) {
				shift();
			}
			while(!is_ended && shifter.get() == 0b11111'11111) {
				shift();
			}

			// Type should be 8 for a header, 7 for some data.
			return byte();
		};

		while(!is_ended && decoded.size() != size_t(extent.number_of_sectors)) {
			// Find a header.
			const auto header_start = block_type();
			if(header_start != 0x8) {
				continue;
			}
			const auto checksum = byte();
			const auto sector_id = byte();
			const auto track_id = byte();
			const auto disk_id1 = byte();
			const auto disk_id2 = byte();

			if(checksum != (sector_id ^ track_id ^ disk_id1 ^ disk_id2)) {
				continue;
			}
			if(sector_id >= extent.number_of_sectors) {
				continue;
			}

			// Skip to data.
			const auto data_start = block_type();
			if(data_start != 0x7) {
				continue;
			}

			// Copy into place if not yet present.
			uint8_t data_checksum = 0;
			std::vector<uint8_t> sector_contents(256);
			for(size_t c = 0; c < 256; c++) {
				const uint8_t next = byte();
				data_checksum ^= next;
				sector_contents[c] = next;
			}

			if(byte() != data_checksum) {
				continue;
			}
			decoded.emplace(sector_id, std::move(sector_contents));
		}

		// Write.
		std::lock_guard lock_guard(file_.file_access_mutex());
		for(auto &[sector, contents]: decoded) {
			file_.seek(extent.file_offset + sector * 256, Whence::SET);
			file_.write(contents);
		}
	}
}

