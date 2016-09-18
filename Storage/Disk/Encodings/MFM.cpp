//
//  MFM.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/09/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "MFM.hpp"

using namespace Storage::Encodings;

void Shifter::add_sync()
{
	// i.e. 0100 0100 1000 1001
	output = (uint16_t)((output << 15) | 0x4489);
}

void MFMShifter::shift(uint8_t input)
{
	uint16_t spread_value =
		(uint16_t)(
			((input & 0x01) << 0) |
			((input & 0x02) << 1) |
			((input & 0x04) << 2) |
			((input & 0x08) << 3) |
			((input & 0x10) << 4) |
			((input & 0x20) << 5) |
			((input & 0x40) << 6) |
			((input & 0x80) << 7)
		);
	uint16_t or_bits = (uint16_t)((spread_value << 1) | (spread_value >> 1) | (output << 15));
	output = spread_value | ((~or_bits) & 0xaaaa);
}

void FMShifter::shift(uint8_t input)
{
	output =
		(uint16_t)(
			((input & 0x01) << 0) |
			((input & 0x02) << 1) |
			((input & 0x04) << 2) |
			((input & 0x08) << 3) |
			((input & 0x10) << 4) |
			((input & 0x20) << 5) |
			((input & 0x40) << 6) |
			((input & 0x80) << 7) |
			0xaaaa
		);
}
