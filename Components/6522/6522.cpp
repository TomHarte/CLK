//
//  6522.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/06/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "6522.hpp"

using namespace MOS;

MOS6522::MOS6522()
{
}

void MOS6522::set_register(int address, uint8_t value)
{
}

uint8_t MOS6522::get_register(int address)
{
	return 0xff;
}
