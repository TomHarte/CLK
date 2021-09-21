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
	if(index) {
		line_mode_ = !(value & 1);
	} else {

	}
	shifts_[index] = value >> 12;
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
	if(!height_) return false;

	if(line_mode_) {
		//
		// Line mode.
		//

		// Bluffer's guide to line mode:
		//
		// In Bresenham terms, the following registers have been set up:
		//
		//	[A modulo] = 4 * (dy - dx)
		//	[B modulo] = 4 * dy
		//	[A pointer] = 4 * dy - 2 * dx, with the sign flag in BLTCON1 indicating sign.
		//
		//	[A data] = 0x8000
		//	[Both masks] = 0xffff
		//	[A shift] = x1 & 15
		//
		//	[B data] = texture
		//	[B shift] = bit at which to start the line texture (0 = LSB)
		//
		//	[C and D pointers] = word containing the first pixel of the line
		//	[C and D modulo] = width of the bitplane in bytes
		//
		//	height = number of pixels
		//
		//	If ONEDOT of BLTCON1 is set, plot only a single bit per horizontal row.
		//
		//	BLTCON1 quadrants are (bits 2–4):
		//
		//		110 -> step in x, x positive, y negative
		//		111 -> step in x, x negative, y negative
		//		101 -> step in x, x negative, y positive
		//		100 -> step in x, x positive, y positive
		//
		//		001 -> step in y, x positive, y negative
		//		011 -> step in y, x negative, y negative
		//		010 -> step in y, x negative, y positive
		//		000 -> step in y, x positive, y positive
		//
		//	So that's:
		//
		//		* bit 4 = x or y major;
		//		* bit 3 = 1 => major variable negative; otherwise positive;
		//		* bit 2 = 1 => minor variable negative; otherwise positive.

		printf("!!! Line %08x\n", pointer_[3]);
//		ram_[pointer_[3] & ram_mask_] = 0x0001 << shifts_[0];
	} else {
		// Copy mode.
		printf("!!! Copy %08x\n", pointer_[3]);
	}

	ram_[pointer_[3] & ram_mask_] = 0xffff;
	height_ = 0;

	return true;
}
