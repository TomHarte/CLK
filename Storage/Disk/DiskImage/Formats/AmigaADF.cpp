//
//  AmigaADF.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/07/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "AmigaADF.hpp"

#include "../../Encodings/MFM/Constants.hpp"
#include "../../Encodings/MFM/Encoder.hpp"
#include "../../Track/PCMTrack.hpp"

using namespace Storage::Disk;

namespace {

template <typename IteratorT> void write_block(IteratorT begin, IteratorT end, std::unique_ptr<Storage::Encodings::MFM::Encoder> &encoder) {
	// Parse 1: write odd bits.
	auto cursor = begin;
	while(cursor != end) {
		auto source = uint8_t(
			((*cursor & 0x02) << 3) |
			((*cursor & 0x08) << 2) |
			((*cursor & 0x20) << 1) |
			((*cursor & 0x80) << 0)
		);
		++cursor;
		source |=
			((*cursor & 0x02) >> 1) |
			((*cursor & 0x08) >> 2) |
			((*cursor & 0x20) >> 3) |
			((*cursor & 0x80) >> 4);
		++cursor;

		encoder->add_byte(source);
	}

	// Parse 2: write even bits.
	cursor = begin;
	while(cursor != end) {
		auto source = uint8_t(
			((*cursor & 0x01) << 4) |
			((*cursor & 0x04) << 3) |
			((*cursor & 0x10) << 2) |
			((*cursor & 0x40) << 1)
		);
		++cursor;
		source |=
			((*cursor & 0x01) >> 0) |
			((*cursor & 0x04) >> 1) |
			((*cursor & 0x10) >> 2) |
			((*cursor & 0x40) >> 3);
		++cursor;

		encoder->add_byte(source);
	}
}

template <typename IteratorT> std::array<uint8_t, 4> checksum(IteratorT begin, IteratorT end) {
	uint64_t sum = 0;

	while(begin != end) {
		// Pull a big-endian 32-bit number.
		uint32_t next = 0;
		next |= uint64_t(*begin) << 24;	++begin;
		next |= uint64_t(*begin) << 16;	++begin;
		next |= uint64_t(*begin) << 8;	++begin;
		next |= uint64_t(*begin) << 0;	++begin;

		// Add, and then add carry too.
		sum += uint64_t(next);
		sum += (sum >> 32);
		sum &= 0xffff'ffff;
	}

	// Invert.
	sum = ~sum;

	// Pack big-endian.
	return std::array<uint8_t, 4>{
		uint8_t((sum >> 24) & 0xff),
		uint8_t((sum >> 16) & 0xff),
		uint8_t((sum >> 8) & 0xff),
		uint8_t((sum >> 0) & 0xff)
	};
}

}

AmigaADF::AmigaADF(const std::string &file_name) :
		file_(file_name) {
	// Dumb validation only for now: a size check.
	if(file_.stats().st_size != 901120) throw Error::InvalidFormat;
}

HeadPosition AmigaADF::get_maximum_head_position() {
	return HeadPosition(80);
}

int AmigaADF::get_head_count() {
	return 2;
}

std::shared_ptr<Track> AmigaADF::get_track_at_position(Track::Address address) {
	using namespace Storage::Encodings;

	// Create an MFM encoder.
	Storage::Disk::PCMSegment encoded_segment;
	encoded_segment.data.reserve(102'400);	// i.e. 0x1900 bytes.
	auto encoder = MFM::GetMFMEncoder(encoded_segment.data);

	// Each track begins with two sync words.
	encoder->output_short(MFM::MFMSync);
	encoder->output_short(MFM::MFMSync);

	// Grab the unencoded track.
	file_.seek(get_file_offset_for_position(address), SEEK_SET);
	const std::vector<uint8_t> track_data = file_.read(512 * 11);

	// Eleven sectors are then encoded.
	for(size_t s = 0; s < 11; s++) {
		// Two bytes of 0x00 act as an inter-sector gap.
		encoder->add_byte(0);
		encoder->add_byte(0);

		// Add additional sync.
		encoder->output_short(MFM::MFMSync);
		encoder->output_short(MFM::MFMSync);

		// Write the header.
		const uint8_t header[4] = {
			0xff,									// Amiga v1.0 format.
			uint8_t(address.position.as_int()),		// Track.
			uint8_t(s),								// Sector.
			uint8_t(11 - s),						// Sectors remaining.
		};
		write_block(header, &header[4], encoder);

		// Write the sector label.
		const std::array<uint8_t, 16> os_recovery{};
		write_block(os_recovery.begin(), os_recovery.end(), encoder);

		// Write checksums.
		const auto header_checksum = checksum(&header[0], &header[4]);
		write_block(header_checksum.begin(), header_checksum.end(), encoder);

		const auto data_checksum = checksum(&track_data[s * 512], &track_data[(s + 1) * 512]);
		write_block(data_checksum.begin(), data_checksum.end(), encoder);

		// Write data.
		write_block(&track_data[s * 512], &track_data[(s + 1) * 512], encoder);
	}

	return std::make_shared<Storage::Disk::PCMTrack>(std::move(encoded_segment));
}

long AmigaADF::get_file_offset_for_position(Track::Address address) {
	return (address.position.as_int() * 2 + address.head) * 512 * 11;
}
