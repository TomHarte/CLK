//
//  WOZ.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/04/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#include "WOZ.hpp"

#include "../../Track/PCMTrack.hpp"

using namespace Storage::Disk;

WOZ::WOZ(const std::string &file_name) :
	file_(file_name) {

	const char signature[8] = {
		'W', 'O', 'Z', '1',
		static_cast<char>(0xff), 0x0a, 0x0d, 0x0a
	};
	if(!file_.check_signature(signature, 8)) throw ErrorNotWOZ;

	// TODO: check CRC32, instead of skipping it.
	file_.seek(4, SEEK_CUR);

	// Parse all chunks up front.
	while(true) {
		const uint32_t chunk_id = file_.get32le();
		const uint32_t chunk_size = file_.get32le();
		if(file_.eof()) break;

		long end_of_chunk = file_.tell() + static_cast<long>(chunk_size);

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
}

int WOZ::get_head_position_count() {
	// TODO: deal with the elephant in the room of non-integral track coordinates.
	return is_3_5_disk_ ? 80 : 160;
}

int WOZ::get_head_count() {
	return is_3_5_disk_ ? 2 : 1;
}

std::shared_ptr<Track> WOZ::get_track_at_position(Track::Address address) {
	// Out-of-bounds => no track.
	if(address.head >= get_head_count()) return nullptr;
	if(address.position >= get_head_position_count()) return nullptr;

	// Calculate table position; if this track is defined to be unformatted, return no track.
	const int table_position = address.head * get_head_position_count() + address.position;
	if(track_map_[table_position] == 0xff) return nullptr;

	// Seek to the real track.
	file_.seek(tracks_offset_ + track_map_[table_position] * 6656, SEEK_SET);

	PCMSegment track_contents;
	track_contents.data = file_.read(6646);
	track_contents.data.resize(file_.get16le());
	track_contents.number_of_bits = file_.get16le();

	const uint16_t splice_point = file_.get16le();
	if(splice_point != 0xffff) {
		// TODO: expand track from splice_point?
	}

	return std::shared_ptr<PCMTrack>(new PCMTrack(track_contents));
}
