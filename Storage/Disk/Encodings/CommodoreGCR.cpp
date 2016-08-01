//
//  CommodoreGCR.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/07/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "CommodoreGCR.hpp"

using namespace Storage;

Time Storage::Encodings::CommodoreGCR::length_of_a_bit_in_time_zone(unsigned int time_zone)
{
	Time duration;
	// the speed zone divides a 4Mhz clock by 13, 14, 15 or 16, with higher-numbered zones being faster (i.e. each bit taking less time)
	duration.length = 16 - time_zone;
	duration.clock_rate = 4000000;
	return duration;
}

unsigned int Storage::Encodings::CommodoreGCR::encoding_for_nibble(uint8_t nibble)
{
	switch(nibble & 0xf)
	{
		case 0x0:	return 0x0a;
		case 0x1:	return 0x0b;
		case 0x2:	return 0x12;
		case 0x3:	return 0x13;
		case 0x4:	return 0x0e;
		case 0x5:	return 0x0f;
		case 0x6:	return 0x16;
		case 0x7:	return 0x17;
		case 0x8:	return 0x09;
		case 0x9:	return 0x19;
		case 0xa:	return 0x1a;
		case 0xb:	return 0x1b;
		case 0xc:	return 0x0d;
		case 0xd:	return 0x1d;
		case 0xe:	return 0x1e;
		case 0xf:	return 0x15;

		// for the benefit of the compiler; clearly unreachable
		default:	return 0xff;
	}
}

unsigned int Storage::Encodings::CommodoreGCR::encoding_for_byte(uint8_t byte)
{
	return encoding_for_nibble(byte) | (encoding_for_nibble(byte >> 4) << 5);
}
