//
//  Blitter.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 22/07/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "Blitter.hpp"

//#define NDEBUG
#define LOG_PREFIX "[Blitter] "
#include "../../Outputs/Log.hpp"

using namespace Amiga;

void Blitter::set_control(int index, uint16_t value) {
	LOG("Set control " << index << " to " << PADHEX(4) << value);
}

void Blitter::set_first_word_mask(uint16_t value) {
	LOG("Set first word mask: " << PADHEX(4) << value);
}

void Blitter::set_last_word_mask(uint16_t value) {
	LOG("Set last word mask: " << PADHEX(4) << value);
}

void Blitter::set_size(uint16_t value) {
	width_ = (width_ & ~0x3f) | (value & 0x3f);
	height_ = (height_ & ~0x3ff) | (value >> 6);
	LOG("Set size to " << std::dec << width_ << ", " << height_);
}

void Blitter::set_minterms(uint16_t value) {
	LOG("Set minterms " << PADHEX(4) << value);
	minterms_ = value & 0xff;
}

void Blitter::set_vertical_size(uint16_t value) {
	LOG("Set vertical size " << PADHEX(4) << value);
}

void Blitter::set_horizontal_size(uint16_t value) {
	LOG("Set horizontal size " << PADHEX(4) << value);
}

void Blitter::set_modulo(int channel, uint16_t value) {
	LOG("Set modulo size " << channel << " to " << PADHEX(4) << value);
}

void Blitter::set_data(int channel, uint16_t value) {
	LOG("Set data " << channel << " to " << PADHEX(4) << value);
}

uint16_t Blitter::get_status() {
	LOG("Returned dummy status");
	return 0;
}

bool Blitter::advance() {
	ram_[pointer_[3] & ram_mask_] = 0xffff;
	return false;
}
