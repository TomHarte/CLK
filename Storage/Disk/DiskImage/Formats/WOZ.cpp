//
//  WOZ.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/04/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#include "WOZ.hpp"

#include "../../Track/PCMTrack.hpp"
#include "../../Track/TrackSerialiser.hpp"

#include <cstring>

using namespace Storage::Disk;

WOZ::WOZ(const std::string &file_name) :
	file_(file_name) {

	const char signature[8] = {
		'W', 'O', 'Z', '1',
		char(0xff), 0x0a, 0x0d, 0x0a
	};
	if(!file_.check_signature(signature, 8)) throw Error::InvalidFormat;

	// Get the file's CRC32.
	const uint32_t crc = file_.get32le();

	// Get the collection of all data that contributes to the CRC.
	post_crc_contents_ = file_.read(size_t(file_.stats().st_size - 12));

	// Test the CRC.
	const uint32_t computed_crc = crc_generator.compute_crc(post_crc_contents_);
	if(crc != computed_crc) {
		 throw Error::InvalidFormat;
	}

	// Retreat to the first byte after the CRC.
	file_.seek(12, SEEK_SET);

	// Parse all chunks up front.
	bool has_tmap = false;
	while(true) {
		const uint32_t chunk_id = file_.get32le();
		const uint32_t chunk_size = file_.get32le();
		if(file_.eof()) break;

		long end_of_chunk = file_.tell() + long(chunk_size);

		#define CK(str) (str[0] | (str[1] << 8) | (str[2] << 16) | (str[3] << 24))
		switch(chunk_id) {
			case CK("INFO"): {
				const uint8_t version = file_.get8();
				if(version != 1) break;
				is_3_5_disk_ = file_.get8() == 2;
				is_read_only_ = file_.get8() == 1;
				/* Ignored:
					1 byte: Synchronized; 1 = Cross track sync was used during imaging.
					1 byte: Cleaned; 1 = MC3470 fake bits have been removed.
					32 bytes: Cretor; a UTF-8 string.
				*/
			} break;

			case CK("TMAP"): {
				file_.read(track_map_, 160);
				has_tmap = true;
			} break;

			case CK("TRKS"): {
				tracks_offset_ = file_.tell();
			} break;

			// TODO: parse META chunks.

			default:
			break;
		}
		#undef CK

		file_.seek(end_of_chunk, SEEK_SET);
	}

	if(tracks_offset_ == -1 || !has_tmap) throw Error::InvalidFormat;
}

HeadPosition WOZ::get_maximum_head_position() {
	return is_3_5_disk_ ? HeadPosition(80) : HeadPosition(160, 4);
}

int WOZ::get_head_count() {
	return is_3_5_disk_ ? 2 : 1;
}

long WOZ::file_offset(Track::Address address) {
	// Calculate table position; if this track is defined to be unformatted, return no track.
	const int table_position = address.head * (is_3_5_disk_ ? 80 : 160) +
		(is_3_5_disk_ ? address.position.as_int() : address.position.as_quarter());
	if(track_map_[table_position] == 0xff) return NoSuchTrack;

	// Seek to the real track.
	return tracks_offset_ + track_map_[table_position] * 6656;
}

std::shared_ptr<Track> WOZ::get_track_at_position(Track::Address address) {
	long offset = file_offset(address);
	if(offset == NoSuchTrack) return nullptr;

	// Seek to the real track.
	std::vector<uint8_t> track_contents;
	size_t number_of_bits;
	{
		std::lock_guard<std::mutex> lock_guard(file_.get_file_access_mutex());
		file_.seek(offset, SEEK_SET);

		// In WOZ a track is up to 6646 bytes of data, followed by a two-byte record of the
		// number of bytes that actually had data in them, then a two-byte count of the number
		// of bits that were used. Other information follows but is not intended for emulation.
		track_contents = file_.read(6646);
		file_.seek(2, SEEK_CUR);
		number_of_bits = std::min(file_.get16le(), uint16_t(6646*8));
	}

	return std::make_shared<PCMTrack>(PCMSegment(number_of_bits, track_contents));
}

void WOZ::set_tracks(const std::map<Track::Address, std::shared_ptr<Track>> &tracks) {
	for(const auto &pair: tracks) {
		// Decode the track and store, patching into the post_crc_contents_.
		auto segment = Storage::Disk::track_serialisation(*pair.second, Storage::Time(1, 50000));

		auto offset = size_t(file_offset(pair.first) - 12);
		std::vector<uint8_t> segment_bytes = segment.byte_data();
		memcpy(&post_crc_contents_[offset - 12], segment_bytes.data(), segment_bytes.size());

		// Write number of bytes and number of bits.
		post_crc_contents_[offset + 6646] = uint8_t(segment.data.size() >> 3);
		post_crc_contents_[offset + 6647] = uint8_t(segment.data.size() >> 11);
		post_crc_contents_[offset + 6648] = uint8_t(segment.data.size());
		post_crc_contents_[offset + 6649] = uint8_t(segment.data.size() >> 8);

		// Set no splice information now provided, since it's been lost if ever it was known.
		post_crc_contents_[offset + 6650] = 0xff;
		post_crc_contents_[offset + 6651] = 0xff;
	}

	// Calculate the new CRC.
	const uint32_t crc = crc_generator.compute_crc(post_crc_contents_);

	// Grab the file lock, then write the CRC, then just dump the entire file buffer.
	std::lock_guard<std::mutex> lock_guard(file_.get_file_access_mutex());
	file_.seek(8, SEEK_SET);
	file_.put_le(crc);
	file_.write(post_crc_contents_);
}

bool WOZ::get_is_read_only() {
	return file_.get_is_known_read_only();
}
