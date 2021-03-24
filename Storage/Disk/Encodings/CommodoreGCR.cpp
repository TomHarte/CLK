//
//  CommodoreGCR.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/07/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "CommodoreGCR.hpp"
#include <limits>

using namespace Storage;

Time Storage::Encodings::CommodoreGCR::length_of_a_bit_in_time_zone(unsigned int time_zone) {
	// the speed zone divides a 4Mhz clock by 13, 14, 15 or 16, with higher-numbered zones being faster (i.e. each bit taking less time)
	return Time(16 - time_zone, 4000000u);
}

unsigned int Storage::Encodings::CommodoreGCR::encoding_for_nibble(uint8_t nibble) {
	switch(nibble & 0xf) {
		case 0x0:	return 0x0a;		case 0x1:	return 0x0b;
		case 0x2:	return 0x12;		case 0x3:	return 0x13;
		case 0x4:	return 0x0e;		case 0x5:	return 0x0f;
		case 0x6:	return 0x16;		case 0x7:	return 0x17;
		case 0x8:	return 0x09;		case 0x9:	return 0x19;
		case 0xa:	return 0x1a;		case 0xb:	return 0x1b;
		case 0xc:	return 0x0d;		case 0xd:	return 0x1d;
		case 0xe:	return 0x1e;		case 0xf:	return 0x15;

		// for the benefit of the compiler; clearly unreachable
		default:	return 0xff;
	}
}

unsigned int Storage::Encodings::CommodoreGCR::decoding_from_quintet(unsigned int quintet) {
	switch(quintet & 0x1f) {
		case 0x0a:	return 0x0;			case 0x0b:	return 0x1;
		case 0x12:	return 0x2;			case 0x13:	return 0x3;
		case 0x0e:	return 0x4;			case 0x0f:	return 0x5;
		case 0x16:	return 0x6;			case 0x17:	return 0x7;
		case 0x09:	return 0x8;			case 0x19:	return 0x9;
		case 0x1a:	return 0xa;			case 0x1b:	return 0xb;
		case 0x0d:	return 0xc;			case 0x1d:	return 0xd;
		case 0x1e:	return 0xe;			case 0x15:	return 0xf;

		default:	return std::numeric_limits<unsigned int>::max();
	}
}

unsigned int Storage::Encodings::CommodoreGCR::encoding_for_byte(uint8_t byte) {
	return encoding_for_nibble(byte) | (encoding_for_nibble(byte >> 4) << 5);
}

unsigned int Storage::Encodings::CommodoreGCR::decoding_from_dectet(unsigned int dectet) {
	return decoding_from_quintet(dectet) | (decoding_from_quintet(dectet >> 5) << 4);
}

void Storage::Encodings::CommodoreGCR::encode_block(uint8_t *destination, uint8_t *source) {
	unsigned int encoded_bytes[4] = {
		encoding_for_byte(source[0]),
		encoding_for_byte(source[1]),
		encoding_for_byte(source[2]),
		encoding_for_byte(source[3]),
	};

	destination[0] = uint8_t(encoded_bytes[0] >> 2);
	destination[1] = uint8_t((encoded_bytes[0] << 6) | (encoded_bytes[1] >> 4));
	destination[2] = uint8_t((encoded_bytes[1] << 4) | (encoded_bytes[2] >> 6));
	destination[3] = uint8_t((encoded_bytes[2] << 2) | (encoded_bytes[3] >> 8));
	destination[4] = uint8_t(encoded_bytes[3]);
}
