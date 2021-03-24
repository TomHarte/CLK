//
//  DMK.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 08/01/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#include "DMK.hpp"

#include "../../Encodings/MFM/Constants.hpp"
#include "../../Encodings/MFM/Encoder.hpp"
#include "../../Track/PCMTrack.hpp"

using namespace Storage::Disk;

namespace  {

std::unique_ptr<Storage::Encodings::MFM::Encoder> new_encoder(Storage::Disk::PCMSegment &segment, bool is_double_density) {
	std::unique_ptr<Storage::Encodings::MFM::Encoder> encoder;

	if(is_double_density) {
		encoder = Storage::Encodings::MFM::GetMFMEncoder(segment.data);
		segment.length_of_a_bit = Storage::Encodings::MFM::MFMBitLength;
	} else {
		encoder = Storage::Encodings::MFM::GetFMEncoder(segment.data);
		segment.length_of_a_bit = Storage::Encodings::MFM::FMBitLength;
	}

	return encoder;
}

}

DMK::DMK(const std::string &file_name) :
	file_(file_name) {
	// Determine whether this DMK represents a read-only disk (whether intentionally,
	// or by virtue of filesystem placement).
	uint8_t read_only_byte = file_.get8();
	if(read_only_byte != 0x00 && read_only_byte != 0xff) throw Error::InvalidFormat;
	is_read_only_ = (read_only_byte == 0xff) || file_.get_is_known_read_only();

	// Read track count and size.
	head_position_count_ = int(file_.get8());
	track_length_ = long(file_.get16le());

	// Track length must be at least 0x80, as that's the size of the IDAM
	// table before track contents.
	if(track_length_ < 0x80) throw Error::InvalidFormat;

	// Read the file flags and apply them.
	uint8_t flags = file_.get8();
	head_count_ = 2 - ((flags & 0x10) >> 4);
	head_position_count_ /= head_count_;
	is_purely_single_density_ = !!(flags & 0x40);

	// Skip to the end of the header and check that this is
	// "in the emulator's native format".
	file_.seek(0xc, SEEK_SET);
	uint32_t format = file_.get32le();
	if(format) throw Error::InvalidFormat;
}

HeadPosition DMK::get_maximum_head_position() {
	return HeadPosition(head_position_count_);
}

int DMK::get_head_count() {
	return head_count_;
}

bool DMK::get_is_read_only() {
	return true;
	// Given that track serialisation is not yet implemented, treat all DMKs as read-only.
//	return is_read_only_;
}

long DMK::get_file_offset_for_position(Track::Address address) {
	return (address.head*head_count_ + address.position.as_int()) * track_length_ + 16;
}

std::shared_ptr<::Storage::Disk::Track> DMK::get_track_at_position(::Storage::Disk::Track::Address address) {
	file_.seek(get_file_offset_for_position(address), SEEK_SET);

	// Read the IDAM table.
	uint16_t idam_locations[64];
	std::size_t idam_count = 0;
	for(std::size_t c = 0; c < sizeof(idam_locations) / sizeof(*idam_locations); ++c) {
		idam_locations[idam_count] = file_.get16le();
		if((idam_locations[idam_count] & 0x7fff) >= 128) {
			idam_count++;
		}
	}

	// Grab the rest of the track.
	std::vector<uint8_t> track = file_.read(size_t(track_length_ - 0x80));

	// Default to outputting double density unless the disk doesn't support it.
	bool is_double_density = !is_purely_single_density_;
	std::vector<PCMSegment> segments;
	std::unique_ptr<Encodings::MFM::Encoder> encoder;
	segments.emplace_back();
	encoder = new_encoder(segments.back(), is_double_density);

	std::size_t idam_pointer = 0;

	const std::size_t track_length = size_t(track_length_) - 0x80;
	std::size_t track_pointer = 0;
	while(track_pointer < track_length) {
		// Determine bytes left until next IDAM.
		std::size_t destination;
		if(idam_pointer != idam_count) {
			destination = (idam_locations[idam_pointer] & 0x7fff) - 0x80;
		} else {
			destination = track_length;
		}

		// Output every intermediate byte.
		if(!is_double_density && !is_purely_single_density_) {
			is_double_density = true;
			segments.emplace_back();
			encoder = new_encoder(segments.back(), is_double_density);
		}
		while(track_pointer < destination) {
			encoder->add_byte(track[track_pointer]);
			track_pointer++;
		}

		// Exit now if that's it.
		if(destination == track_length) break;

		// Being now located at the IDAM, check for a change of encoding.
		bool next_is_double_density = !!(idam_locations[idam_pointer] & 0x8000);
		if(next_is_double_density != is_double_density) {
			is_double_density = next_is_double_density;
			segments.emplace_back();
			encoder = new_encoder(segments.back(), is_double_density);
		}

		// Now at the IDAM, which will always be an FE regardless of FM/MFM encoding,
		// presumably through misunderstanding of the designer? Write out a real IDAM
		// for the current density, then the rest of the ID: four bytes for the address
		// plus two for the CRC. Keep a copy of the header while we're here, so that the
		// size of the sector is known momentarily.
		std::size_t step_rate = (!is_double_density && !is_purely_single_density_) ? 2 : 1;
		encoder->add_ID_address_mark();
		uint8_t header[6];
		for(int c = 0; c < 6; ++c) {
			track_pointer += step_rate;
			encoder->add_byte(track[track_pointer]);
			header[c] = track[track_pointer];
		}
		track_pointer += step_rate;

		// Now write out as many bytes as are found prior to an FB or F8 (same comment as
		// above: those are the FM-esque marks, but it seems as though transcription to MFM
		// is implicit).
		while(true) {
			uint8_t next_byte = track[track_pointer];
			track_pointer += step_rate;
			if(next_byte == 0xfb || next_byte == 0xf8) {
				// Write a data or deleted data address mark.
				if(next_byte == 0xfb) encoder->add_data_address_mark();
				else encoder->add_deleted_data_address_mark();
				break;
			}
			encoder->add_byte(next_byte);
		}

		// Now write out a data mark (the file format appears to leave these implicit?),
		// then the sector contents plus the CRC.
		encoder->add_data_address_mark();
		int sector_size = 2 + (128 << header[3]);
		while(sector_size--) {
			encoder->add_byte(track[track_pointer]);
			track_pointer += step_rate;
		}

		idam_pointer++;
	}

	return std::make_shared<PCMTrack>(segments);
}
