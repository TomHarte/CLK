//
//  CommodoreROM.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/09/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "CommodoreROM.hpp"

bool Storage::Cartridge::Encodings::CommodoreROM::isROM(const std::vector<uint8_t> &contents)
{
	return
		(
			contents.size() == 0x400 ||
			contents.size() == 0x800 ||
			contents.size() == 0x1000 ||
			contents.size() == 0x2000
		) &&
		contents[4] == 0x41 &&
		contents[5] == 0x30 &&
		contents[6] == 0xc3 &&
		contents[7] == 0xc2 &&
		contents[8] == 0xcd;
}
