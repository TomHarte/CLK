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
	segment.data.reserve(size_t(length * bit_size));

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
	data[index+0] = uint8_t(((value) >> 1) | 0xaa);	\
	data[index+1] = uint8_t((value) | 0xaa);	\

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

	// TODO: encode.
	(void)source;

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
			uint8_t(
				bit_reverse[source[c]&3] |
				(bit_reverse[source[c + 86]&3] << 2) |
				(bit_reverse[source[c + 172]&3] << 4)
			);
	}
	data[87] =
			uint8_t(
				(bit_reverse[source[84]&3] << 0) |
				(bit_reverse[source[170]&3] << 2)
			);
	data[88] =
			uint8_t(
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

Storage::Disk::PCMSegment AppleGCR::Macintosh::header(uint8_t type, uint8_t track, uint8_t sector, bool side_two) {
	std::vector<uint8_t> data(11);

	// The standard prologue.
	data[0] = header_prologue[0];
	data[1] = header_prologue[1];
	data[2] = header_prologue[2];

	// There then follows:
	//
	//	1) the low six bits of the track number;
	//	2) the sector number;
	//	3) the high five bits of the track number plus a side flag;
	//	4) the type; and
	//	5) the XOR of all those fields.
	//
	//	(all two-and-six encoded).
	data[3] = track&0x3f;
	data[4] = sector;
	data[5] = (side_two ? 0x20 : 0x00) | ((track >> 6) & 0x1f);
	data[6] = type;
	data[7] = data[3] ^ data[4] ^ data[5] ^ data[6];

	for(size_t c = 3; c < 8; ++c) {
		data[c] = six_and_two_mapping[data[c]];
	}

	// Then the standard epilogue.
	data[8] = epilogue[0];
	data[9] = epilogue[1];
	data[10] = epilogue[2];

	return Storage::Disk::PCMSegment(data);
}

Storage::Disk::PCMSegment AppleGCR::Macintosh::data(uint8_t sector, const uint8_t *source) {
	std::vector<uint8_t> output(710);
	int checksum[3] = {0, 0, 0};

	// Write prologue.
	output[0] = data_prologue[0];
	output[1] = data_prologue[1];
	output[2] = data_prologue[2];

	// Add the sector number.
	output[3] = six_and_two_mapping[sector & 0x3f];

	// The Macintosh has a similar checksum-as-it-goes approach to encoding
	// to the Apple II, but works entirely differently. Each three bytes of
	// input are individually encoded to four GCR bytes, their output values
	// being a (mutating) function of the current checksum.
	//
	// Address references below, such as 'Cf. 18FA4' are to addresses in the
	// Macintosh Plus ROM.
	for(size_t c = 0; c < 175; ++c) {
		uint8_t values[3];

		// The low byte of the checksum is rotated left one position; Cf. 18FA4.
		checksum[0] = (checksum[0] << 1) | (checksum[0] >> 7);

		// See 18FBA and 18FBC: an ADDX (with the carry left over from the roll)
		// and an EOR act to update the checksum and generate the next output.
		values[0] = uint8_t(*source ^ checksum[0]);
		checksum[2] += *source + (checksum[0] >> 8);
		++source;

		// As above, but now 18FD0 and 18FD2.
		values[1] = uint8_t(*source ^ checksum[2]);
		checksum[1] += *source + (checksum[2] >> 8);
		++source;

		// Avoid a potential read overrun, but otherwise continue as before.
		if(c == 174) {
			values[2] = 0;
		} else {
			values[2] = uint8_t(*source ^ checksum[1]);
			checksum[0] += *source + (checksum[1] >> 8);
			++source;
		}

		// Throw away the top bits of checksum[1] and checksum[2]; the original
		// routine is byte centric, the longer ints here are just to retain the
		// carry after each add transientliy.
		checksum[0] &= 0xff;
		checksum[1] &= 0xff;
		checksum[2] &= 0xff;

		// Having mutated those three bytes according to the current checksum,
		// and the checksum according to those bytes, run them through the
		// GCR conversion table.
		output[4 + c*4 + 1] = six_and_two_mapping[values[0] & 0x3f];
		output[4 + c*4 + 2] = six_and_two_mapping[values[1] & 0x3f];
		output[4 + c*4 + 3] = six_and_two_mapping[values[2] & 0x3f];
		output[4 + c*4 + 0] = six_and_two_mapping[
			((values[0] >> 2) & 0x30) |
			((values[1] >> 4) & 0x0c) |
			((values[2] >> 6) & 0x03)
		];
	}

	// Also write the checksum.
	//
	// Caveat: the first byte written here will overwrite the final byte that
	// was deposited in the loop above. That's deliberate. The final byte from
	// the loop above doesn't contain any useful content, and isn't actually
	// included on disk.
	output[704] = six_and_two_mapping[checksum[2] & 0x3f];
	output[705] = six_and_two_mapping[checksum[1] & 0x3f];
	output[706] = six_and_two_mapping[checksum[0] & 0x3f];
	output[703] = six_and_two_mapping[
		((checksum[2] >> 2) & 0x30) |
		((checksum[1] >> 4) & 0x0c) |
		((checksum[0] >> 6) & 0x03)
	];

	// Write epilogue.
	output[707] = epilogue[0];
	output[708] = epilogue[1];
	output[709] = epilogue[2];

	return Storage::Disk::PCMSegment(output);
}
