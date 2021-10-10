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

#include <type_traits>

using namespace Storage::Disk;

namespace {

template <typename IteratorT, class OutputIt> void encode_block(IteratorT begin, IteratorT end, OutputIt first) {
	// Parse 1: combine odd bits.
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

		*first = source;
		++first;
	}

	// Parse 2: combine even bits.
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

		*first = source;
		++first;
	}
}

template <typename IteratorT> void write(IteratorT begin, IteratorT end, std::unique_ptr<Storage::Encodings::MFM::Encoder> &encoder) {
	while(begin != end) {
		encoder->add_byte(*begin);
		++begin;
	}
}

template <typename IteratorT> auto checksum(IteratorT begin, IteratorT end) {
	uint16_t checksum[2]{};
	int offset = 0;
	while(begin != end) {
		const uint8_t value = *begin;
		++begin;

		// Do a clockless MFM encode.
		const auto spread = uint16_t(
			((value&0x80) << 7) |
			((value&0x40) << 6) |
			((value&0x20) << 5) |
			((value&0x10) << 4) |
			((value&0x08) << 3) |
			((value&0x04) << 2) |
			((value&0x02) << 1) |
			((value&0x01) << 0)
		);
		checksum[offset] ^= spread;
		offset ^= 1;
	}

	return std::array<uint8_t, 4>{
		uint8_t(checksum[0] >> 8),
		uint8_t(checksum[0]),
		uint8_t(checksum[1] >> 8),
		uint8_t(checksum[1]),
	};
}

template <typename IteratorT> void write_checksum(IteratorT begin, IteratorT end, std::unique_ptr<Storage::Encodings::MFM::Encoder> &encoder) {
	// Believe it or not, this appears to be the actual checksum algorithm on the Amiga:
	//
	//	(1) calculate the XOR checksum of the MFM-encoded data, read as 32-bit words;
	//	(2) throw away the clock bits;
	//	(3) Take the resulting 32-bit value and perform an odd-even MFM encoding on those.
	const auto raw_checksum = checksum(begin, end);

	std::decay_t<decltype(raw_checksum)> encoded_checksum{};
	encode_block(raw_checksum.begin(), raw_checksum.end(), encoded_checksum.begin());

	write(encoded_checksum.begin(), encoded_checksum.end(), encoder);
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

		// Encode and write the header.
		const uint8_t header[4] = {
			0xff,														// Amiga v1.0 format.
			uint8_t(address.position.as_int() * 2 + address.head),		// Track.
			uint8_t(s),													// Sector.
			uint8_t(11 - s),											// Sectors remaining.
		};
		std::array<uint8_t, 4> encoded_header;
		encode_block(std::begin(header), std::end(header), std::begin(encoded_header));
		write(std::begin(encoded_header), std::end(encoded_header), encoder);

		// Write the sector label.
		const std::array<uint8_t, 16> os_recovery{};
		write(os_recovery.begin(), os_recovery.end(), encoder);

		// Encode the data.
		std::array<uint8_t, 512> encoded_data;
		encode_block(&track_data[s * 512], &track_data[(s + 1) * 512], std::begin(encoded_data));

		// Write checksums.
		write_checksum(std::begin(encoded_header), std::end(encoded_header), encoder);
		write_checksum(std::begin(encoded_data), std::end(encoded_data), encoder);

		// Write data.
		write(std::begin(encoded_data), std::end(encoded_data), encoder);
	}

	return std::make_shared<Storage::Disk::PCMTrack>(std::move(encoded_segment));
}

long AmigaADF::get_file_offset_for_position(Track::Address address) {
	return (address.position.as_int() * 2 + address.head) * 512 * 11;
}
