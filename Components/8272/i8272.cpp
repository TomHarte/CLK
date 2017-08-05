//
//  i8272.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/08/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "i8272.hpp"

using namespace Intel;

void i8272::set_register(int address, uint8_t value) {
}

uint8_t i8272::get_register(int address) {
	return 0xff;
}
