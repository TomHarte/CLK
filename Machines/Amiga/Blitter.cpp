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

Blitter::Blitter(uint16_t *ram, size_t size) : ram_(ram), ram_size_(size) {}

void Blitter::set_control(int index, uint16_t value) {
	LOG("Set control " << index << " to " << PADHEX(4) << value);
}

void Blitter::set_first_word_mask(uint16_t value) {
	LOG("Set first word mask: " << PADHEX(4) << value);
}

void Blitter::set_last_word_mask(uint16_t value) {
	LOG("Set last word mask: " << PADHEX(4) << value);
}

void Blitter::set_pointer(int channel, int shift, uint16_t value) {
	LOG("Set pointer " << channel << " shift " << shift << " to " << PADHEX(4) << value);
}

void Blitter::set_size(uint16_t value) {
	LOG("Set size " << PADHEX(4) << value);
}

void Blitter::set_minterms(uint16_t value) {
	LOG("Set minterms " << PADHEX(4) << value);
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

Cycles Blitter::get_remaining_cycles() {
	return Cycles(0);
}

void Blitter::run_for(Cycles) {
}
