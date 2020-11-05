//
//  Sound.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/11/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#include "Sound.hpp"

#include <cstdio>

using namespace Apple::IIgs::Sound;

void GLU::set_control(uint8_t control) {
	printf("UNIMPLEMENTED: set control %02x\n", control);
}

uint8_t GLU::get_control() {
	return 0;
}

void GLU::set_data(uint8_t data) {
	printf("UNIMPLEMENTED: set data %02x\n", data);
}

uint8_t GLU::get_data() {
	return 0;
}

void GLU::set_address_low(uint8_t low) {
	address_ = uint16_t((address_ & 0xff00) | low);
}

uint8_t GLU::get_address_low() {
	return address_ & 0xff;
}

void GLU::set_address_high(uint8_t high) {
	address_ = uint16_t((high << 8) | (address_ & 0x00ff));
}

uint8_t GLU::get_address_high() {
	return address_ >> 8;
}
