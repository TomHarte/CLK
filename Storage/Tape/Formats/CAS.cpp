//
//  CAS.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/11/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "CAS.hpp"

#include <cstring>

using namespace Storage::Tape;

namespace  {
	const uint8_t header_signature[8] = {0x1f, 0xa6, 0xde, 0xba, 0xcc, 0x13, 0x7d, 0x74};
}

CAS::CAS(const char *file_name) :
	file_(file_name) {
	file_.read(input_, sizeof(input_));
	if(std::memcmp(input_, header_signature, sizeof(header_signature))) throw ErrorNotCAS;
}

bool CAS::is_at_end() {
	return file_.eof();
}

void CAS::virtual_reset() {
	file_.seek(0, SEEK_SET);
	file_.read(input_, sizeof(input_));
	phase_ = Phase::Header;
	distance_into_phase_ = 0;
}

Tape::Pulse CAS::virtual_get_next_pulse() {
	Pulse pulse;
	switch(phase_) {
		case Phase::Gap: {
			// Leave a fixed 0.5 second gap between blocks.
			pulse.length.length = 2400;
			pulse.length.clock_rate = 4800;
			pulse.type = Pulse::Type::Zero;

			if(!file_.eof()) phase_ = Phase::Header;
		} break;

		case Phase::Header: {
			// 1 bits are two complete cycle at 2400 hz
			pulse.length.length = 1;
			pulse.length.clock_rate = 4800;
			pulse.type = (distance_into_phase_&1) ? Pulse::Type::Low : Pulse::Type::High;

			distance_into_phase_++;
			if(distance_into_phase_ == 7936*4) {
				distance_into_phase_ = 0;
				phase_ = Phase::Bytes;

				for(int c = 1; c < 8; c++) {
					input_[c] = file_.get8();
				}
			}
		} break;

		case Phase::Bytes: {
			unsigned int bit;
			const int bit_offset = distance_into_phase_ >> 2;
			switch(bit_offset) {
				case 0:		bit = 0;									break;
				default:	bit = (input_[0] >> (bit_offset - 1)) & 1;	break;
				case 9:
				case 10:	bit = 1;									break;
			}

			// 1 bits are two complete cycle at 2400 hz; 0 bits are one complete cycle at 1200 hz
			pulse.length.length = 2 - bit;
			pulse.length.clock_rate = 4800;
			pulse.type = distance_into_phase_ ? Pulse::Type::High : Pulse::Type::Low;

			int adder = 1;
			if(!bit && ((distance_into_phase_&3) == 1)) adder = 3;
			distance_into_phase_ = (distance_into_phase_ + adder) % (11 * 4);
		} break;
	}

//	if(pulse.type == Pulse::Type::Zero) printf("\n---\n");
//	else {
//		if(pulse.length.length == 1) printf(".");
//		else if(pulse.length.length == 2) printf("+");
//		else printf("?");
//	}

	if(!distance_into_phase_ && phase_ == Phase::Bytes) {
		std::memmove(input_, &input_[1], 7);
		input_[7] = file_.get8();
		if(!is_at_end()) {
			if(!std::memcmp(input_, header_signature, sizeof(header_signature))) {
				phase_ = Phase::Header;//Gap;
			}
		}
	}

	return pulse;
}
