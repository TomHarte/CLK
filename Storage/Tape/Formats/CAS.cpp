//
//  CAS.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/11/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "CAS.hpp"

#include <cassert>
#include <cstring>

using namespace Storage::Tape;

/*
	CAS files are a raw byte capture of tape content, with all solid tones transmuted to
	the placeholder 1F A6 DE BA CC 13 7D 74 and gaps omitted.

	Since that byte stream may also occur within files, and gaps and tone lengths need to be
	reconstructed, knowledge of the MSX tape byte format is also required. Specifically:

	Each tone followed by ten bytes that determine the file type:

		ten bytes of value 0xD0 => a binary file;
		ten bytes of value 0xD3 => it's a basic file;
		ten bytes of value 0xEA => it's an ASCII file; and
		any other pattern implies a raw data block.

	Raw data blocks contain their two-byte length, then data.

	Binary, Basic and ASCII files then have a six-byte file name, followed by a short tone, followed
	by the file contents.

	ASCII files:

		... are a sequence of short tone/256-byte chunk pairs. For CAS purposes, these continue until
		you hit another 1F A6 DE BA CC 13 7D 74 sequence.

	Binary files:

		... begin with three 16-bit values, the starting, ending and execution addresses. Then there is
		the correct amount of data to fill memory from the starting to the ending address, inclusive.

	BASIC files:

		... are in Microsoft-standard BASIC form of (two bytes link to next line), (two bytes line number), [tokens],
		starting from address 0x8001. These files continue until a next line address of 0x0000 is found, then
		are usually padded by 0s for a period that I haven't yet determined a pattern for. The code below treats
		everything to the next 0x1f as padding.
*/

namespace  {
	const uint8_t header_signature[8] = {0x1f, 0xa6, 0xde, 0xba, 0xcc, 0x13, 0x7d, 0x74};

	#define TenX(x) {x, x, x, x, x, x, x, x, x, x}
	const uint8_t binary_signature[] = TenX(0xd0);
	const uint8_t basic_signature[] = TenX(0xd3);
	const uint8_t ascii_signature[] = TenX(0xea);
}

CAS::CAS(const std::string &file_name) {
	Storage::FileHolder file(file_name);

	enum class Mode {
		Seeking,
		ASCII,
		Binary,
		BASIC
	} parsing_mode_ = Mode::Seeking;

	while(true) {
		// Churn through the file until the next header signature is found.
		const auto header_position = file.tell();
		const auto signature = file.read(8);
		if(signature.size() != 8) break;
		if(std::memcmp(signature.data(), header_signature, 8)) {
			if(!chunks_.empty()) chunks_.back().data.push_back(signature[0]);

			// Check for other 1fs in this stream, and repeat from there if any.
			for(size_t c = 1; c < 8; ++c) {
				if(signature[c] == 0x1f) {
					file.seek(header_position + long(c), SEEK_SET);
					break;
				} else {
					// Attach any unexpected bytes to the back of the most recent chunk.
					// In effect this creates a linear search for the next explicit tone.
					if(!chunks_.empty()) {
						chunks_.back().data.push_back(signature[c]);
					}
				}
			}
			continue;
		}

		// A header has definitely been found. Require from here at least 16 further bytes,
		// being the type and a name.
		const auto type = file.read(10);
		if(type.size() != 10) break;

		const bool is_binary	= !std::memcmp(type.data(), binary_signature, type.size());
		const bool is_basic		= !std::memcmp(type.data(), basic_signature, type.size());
		const bool is_ascii		= !std::memcmp(type.data(), ascii_signature, type.size());

		switch(parsing_mode_) {
			case Mode::Seeking: {
				if(is_ascii || is_binary || is_basic) {
					file.seek(header_position + 8, SEEK_SET);
					chunks_.emplace_back(!chunks_.empty(), true, file.read(10 + 6));

					if(is_ascii)	parsing_mode_ = Mode::ASCII;
					if(is_binary)	parsing_mode_ = Mode::Binary;
					if(is_basic)	parsing_mode_ = Mode::BASIC;
				} else {
					// Raw data appears now. Grab its length and keep going.
					file.seek(header_position + 8, SEEK_SET);
					const uint16_t length = file.get16le();

					file.seek(header_position + 8, SEEK_SET);
					chunks_.emplace_back(false, false, file.read(size_t(length) + 2));
				}
			} break;

			case Mode::ASCII:
				// Keep reading ASCII in 256-byte segments until a non-ASCII chunk arrives.
				if(is_binary || is_basic || is_ascii) {
					file.seek(header_position, SEEK_SET);
					parsing_mode_ = Mode::Seeking;
				} else {
					file.seek(header_position + 8, SEEK_SET);
					chunks_.emplace_back(false, false, file.read(256));
				}
			break;

			case Mode::Binary: {
				// Get the start and end addresses in order to figure out how much data
				// is here.
				file.seek(header_position + 8, SEEK_SET);
				const uint16_t start_address = file.get16le();
				const uint16_t end_address = file.get16le();

				file.seek(header_position + 8, SEEK_SET);
				const auto length = end_address - start_address + 1;
				chunks_.emplace_back(false, false, file.read(size_t(length) + 6));

				parsing_mode_ = Mode::Seeking;
			} break;

			case Mode::BASIC: {
				// Horror of horrors, this will mean actually following the BASIC
				// linked list of line contents.
				file.seek(header_position + 8, SEEK_SET);
				uint16_t address = 0x8001;	// the BASIC start address.
				while(true) {
					const uint16_t next_line_address = file.get16le();
					if(!next_line_address || file.eof()) break;
					file.seek(next_line_address - address - 2, SEEK_CUR);
					address = next_line_address;
				}
				const auto length = (file.tell() - 1) - (header_position + 8);

				// Create the chunk and return to regular parsing.
				file.seek(header_position + 8, SEEK_SET);
				chunks_.emplace_back(false, false, file.read(size_t(length)));
				parsing_mode_ = Mode::Seeking;
			} break;
		}
	}
}

bool CAS::is_at_end() {
	return phase_ == Phase::EndOfFile;
}

void CAS::virtual_reset() {
	phase_ = Phase::Header;
	chunk_pointer_ = 0;
	distance_into_phase_ = 0;
	distance_into_bit_ = 0;
}

Tape::Pulse CAS::virtual_get_next_pulse() {
	Pulse pulse;
	pulse.length.clock_rate = 9600;
	// Clock rate is four times the baud rate (of 2400), because the quickest thing that might need
	// to be communicated is a '1', which is two cycles at the baud rate, i.e. four events:
	// high, low, high, low.

	// If this is a gap, then that terminates a file. If this is already the end
	// of the file then perpetual gaps await.
	if(phase_ == Phase::Gap || phase_ == Phase::EndOfFile) {
		pulse.length.length = pulse.length.clock_rate;
		pulse.type = Pulse::Type::Zero;

		if(phase_ == Phase::Gap) {
			phase_ = Phase::Header;
			distance_into_phase_ = 0;
		}

		return pulse;
	}

	// Determine which bit is now forthcoming.
	int bit = 1;

	switch(phase_) {
		default: break;

		case Phase::Header: {
			// In the header, all bits are 1s, so let the default value stand. Just check whether the
			// header is ended and, if so, move on to bytes.
			distance_into_bit_++;
			if(distance_into_bit_ == 2) {
				distance_into_phase_++;
				distance_into_bit_ = 0;

				// This code always produces a 2400 baud signal; so use the appropriate Red Book-supplied
				// constants to check whether the header has come to an end.
				if(distance_into_phase_ == (chunks_[chunk_pointer_].long_header ? 31744 : 7936)) {
					phase_ = Phase::Bytes;
					distance_into_phase_ = 0;
					distance_into_bit_ = 0;
				}
			}
		} break;

		case Phase::Bytes: {
			// Provide bits with a single '0' start bit and two '1' stop bits.
			uint8_t byte_value = chunks_[chunk_pointer_].data[distance_into_phase_ / 11];
			int bit_offset = distance_into_phase_ % 11;
			switch(bit_offset) {
				case 0:		bit = 0;									break;
				default:	bit = (byte_value >> (bit_offset - 1)) & 1;	break;
				case 9:
				case 10:	bit = 1;									break;
			}

			// If bit is finished, and if all bytes in chunk have been posted then:
			//	- if this is the final chunk then note end of file.
			//	- otherwise, roll onto the next header or gap, depending on whether the next chunk has a gap.
			distance_into_bit_++;
			if(distance_into_bit_ == (bit ? 4 : 2)) {
				distance_into_bit_ = 0;
				distance_into_phase_++;
				if(distance_into_phase_ == chunks_[chunk_pointer_].data.size() * 11) {
					distance_into_phase_ = 0;
					chunk_pointer_++;
					if(chunk_pointer_ == chunks_.size()) {
						phase_ = Phase::EndOfFile;
					} else {
						phase_ = chunks_[chunk_pointer_].has_gap ? Phase::Gap : Phase::Header;
					}
				}
			}
		} break;
	}

	// A '1' is encoded with twice the frequency of a '0'.
	pulse.length.length = unsigned(2 - bit);
	pulse.type = (distance_into_bit_ & 1) ? Pulse::Type::High : Pulse::Type::Low;

	return pulse;
}
