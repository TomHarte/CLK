//
//  AppleGCR.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 21/04/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#include "Encoder.hpp"

namespace {

const uint8_t five_and_three_mapping[] = {
	0xab, 0xad, 0xae, 0xaf, 0xb5, 0xb6, 0xb7, 0xba,
	0xbb, 0xbd, 0xbe, 0xbf, 0xd6, 0xd7, 0xda, 0xdb,
	0xdd, 0xde, 0xdf, 0xea, 0xeb, 0xed, 0xee, 0xef,
	0xf5, 0xf6, 0xf7, 0xfa, 0xfb, 0xfd, 0xfe, 0xff
};

const uint8_t six_and_two_mapping[] = {
	0x96, 0x97, 0x9a, 0x9b, 0x9d, 0x9e, 0x9f, 0xa6,
	0xa7, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb2, 0xb3,
	0xb4, 0xb5, 0xb6, 0xb7, 0xb9, 0xba, 0xbb, 0xbc,
	0xbd, 0xbe, 0xbf, 0xcb, 0xcd, 0xce, 0xcf, 0xd3,
	0xd6, 0xd7, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde,
	0xdf, 0xe5, 0xe6, 0xe7, 0xe9, 0xea, 0xeb, 0xec,
	0xed, 0xee, 0xef, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6,
	0xf7, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

/*!
	Produces a PCM segment containing @c length sync bytes, each aligned to the beginning of
	a @c bit_size -sized window.
*/
Storage::Disk::PCMSegment sync(int length, int bit_size) {
	Storage::Disk::PCMSegment segment;

	// Reserve sufficient storage.
	segment.data.reserve(static_cast<size_t>(length * bit_size));

	// Write patters of 0xff padded with 0s to the selected bit size.
	while(length--) {
		int c = 8;
		while(c--)
			segment.data.push_back(true);

		c = bit_size - 8;
		while(c--)
			segment.data.push_back(false);
	}

	return segment;
}

}

using namespace Storage::Encodings;

Storage::Disk::PCMSegment AppleGCR::six_and_two_sync(int length) {
	return sync(length, 10);
}

Storage::Disk::PCMSegment AppleGCR::five_and_three_sync(int length) {
	return sync(length, 9);
}

Storage::Disk::PCMSegment AppleGCR::AppleII::header(uint8_t volume, uint8_t track, uint8_t sector) {
	const uint8_t checksum = volume ^ track ^ sector;

	// Apple headers are encoded using an FM-esque scheme rather than 6 and 2, or 5 and 3.
	std::vector<uint8_t> data(14);

	data[0] = header_prologue[0];
	data[1] = header_prologue[1];
	data[2] = header_prologue[2];

#define WriteFM(index, value)	\
	data[index+0] = static_cast<uint8_t>(((value) >> 1) | 0xaa);	\
	data[index+1] = static_cast<uint8_t>((value) | 0xaa);	\

	WriteFM(3, volume);
	WriteFM(5, track);
	WriteFM(7, sector);
	WriteFM(9, checksum);

#undef WriteFM

	data[11] = epilogue[0];
	data[12] = epilogue[1];
	data[13] = epilogue[2];

	return Storage::Disk::PCMSegment(data);
}

Storage::Disk::PCMSegment AppleGCR::five_and_three_data(const uint8_t *source) {
	std::vector<uint8_t> data(410 + 7);

	data[0] = data_prologue[0];
	data[1] = data_prologue[1];
	data[2] = data_prologue[2];

	data[414] = epilogue[0];
	data[411] = epilogue[1];
	data[416] = epilogue[2];

//	std::size_t source_pointer = 0;
//	std::size_t destination_pointer = 3;
//	while(source_pointer < 255) {
//		encode_five_and_three_block(&segment.data[destination_pointer], &source[source_pointer]);
//
//		source_pointer += 5;
//		destination_pointer += 8;
//	}

	// Map five-bit values up to full bytes.
	for(std::size_t c = 0; c < 410; ++c) {
		data[3 + c] = five_and_three_mapping[data[3 + c]];
	}

	return Storage::Disk::PCMSegment(data);
}

// MARK: - Apple II-specific encoding.

Storage::Disk::PCMSegment AppleGCR::AppleII::six_and_two_data(const uint8_t *source) {
	std::vector<uint8_t> data(349);

	// Add the prologue and epilogue.
	data[0] = data_prologue[0];
	data[1] = data_prologue[1];
	data[2] = data_prologue[2];

	data[346] = epilogue[0];
	data[347] = epilogue[1];
	data[348] = epilogue[2];

	// Fill in byte values: the first 86 bytes contain shuffled
	// and combined copies of the bottom two bits of the sector
	// contents; the 256 bytes afterwards are the remaining
	// six bits.
	const uint8_t bit_reverse[] = {0, 2, 1, 3};
	for(std::size_t c = 0; c < 84; ++c) {
		data[3 + c] =
			static_cast<uint8_t>(
				bit_reverse[source[c]&3] |
				(bit_reverse[source[c + 86]&3] << 2) |
				(bit_reverse[source[c + 172]&3] << 4)
			);
	}
	data[87] =
			static_cast<uint8_t>(
				(bit_reverse[source[84]&3] << 0) |
				(bit_reverse[source[170]&3] << 2)
			);
	data[88] =
			static_cast<uint8_t>(
				(bit_reverse[source[85]&3] << 0) |
				(bit_reverse[source[171]&3] << 2)
			);

	for(std::size_t c = 0; c < 256; ++c) {
		data[3 + 86 + c] = source[c] >> 2;
	}

	// Exclusive OR each byte with the one before it.
	data[345] = data[344];
	std::size_t location = 344;
	while(location > 3) {
		data[location] ^= data[location-1];
		--location;
	}

	// Map six-bit values up to full bytes.
	for(std::size_t c = 0; c < 343; ++c) {
		data[3 + c] = six_and_two_mapping[data[3 + c]];
	}

	return Storage::Disk::PCMSegment(data);
}

// MARK: - Macintosh-specific encoding.

AppleGCR::Macintosh::SectorSpan AppleGCR::Macintosh::sectors_in_track(int track) {
	// A Macintosh disk has 80 tracks, divided into 5 16-track zones. The outermost
	// zone has 12 sectors/track, the next one in has only 11 sectors/track, and
	// that arithmetic progression continues.
	//
	// (... and therefore the elementary sum of an arithmetic progression formula
	// is deployed below)
	const int zone = track >> 4;
	const int prior_sectors = 16 * zone * (12 + (12 - (zone - 1))) / 2;

	AppleGCR::Macintosh::SectorSpan result;
	result.length = 12 - zone;
	result.start = prior_sectors + (track & 15) * result.length;

	return result;
}
