//
//  CAS.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/11/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "CAS.hpp"

#include <cassert>
#include <cstring>

using namespace Storage::Tape;

namespace  {
	const uint8_t header_signature[8] = {0x1f, 0xa6, 0xde, 0xba, 0xcc, 0x13, 0x7d, 0x74};
}

CAS::CAS(const std::string &file_name) {
	Storage::FileHolder file(file_name);
	uint8_t lookahead[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

	// Entirely fill the lookahead and verify that its start matches the header signature.
	get_next(file, lookahead, 10);
	if(std::memcmp(lookahead, header_signature, sizeof(header_signature))) throw ErrorNotCAS;

	while(!file.eof()) {
		// Just found a header, so flush the lookahead.
		get_next(file, lookahead, 8);

		// Create a new chunk
		chunks_.emplace_back();
		Chunk &chunk = chunks_.back();

		// Decide whether to award a long header and/or a gap.
		bool bytes_are_equal = true;
		for(std::size_t index = 0; index < sizeof(lookahead); index++)
			bytes_are_equal &= (lookahead[index] == lookahead[0]);

		chunk.long_header = bytes_are_equal && ((lookahead[0] == 0xd3) || (lookahead[0] == 0xd0) || (lookahead[0] == 0xea));
		chunk.has_gap = chunk.long_header && (chunks_.size() > 1);

		// Keep going until another header arrives or the file ends. Headers require the magic byte sequence,
		// and also must be eight-byte aligned within the file.
		while(	!file.eof() &&
				(std::memcmp(lookahead, header_signature, sizeof(header_signature)) || ((file.tell()-10)&7))) {
			chunk.data.push_back(lookahead[0]);
			get_next(file, lookahead, 1);
		}

		// If the file ended, flush the lookahead. The final thing in it will be a 0xff from the read that
		// triggered the eof, so don't include that.
		if(file.eof()) {
			for(std::size_t index = 0; index < sizeof(lookahead) - 1; index++)
				chunk.data.push_back(lookahead[index]);
		}
	}
}

/*!
	Treating @c buffer as a sliding lookahead, shifts it @c quantity elements to the left and
	populates the new empty area to the right from @c file.
*/
void CAS::get_next(Storage::FileHolder &file, uint8_t (&buffer)[10], std::size_t quantity) {
	assert(quantity <= sizeof(buffer));

	if(quantity < sizeof(buffer))
		std::memmove(buffer, &buffer[quantity], sizeof(buffer) - quantity);

	while(quantity--) {
		buffer[sizeof(buffer) - 1 - quantity] = file.get8();
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
	pulse.length.length = static_cast<unsigned int>(2 - bit);
	pulse.type = (distance_into_bit_ & 1) ? Pulse::Type::High : Pulse::Type::Low;

	return pulse;
}
