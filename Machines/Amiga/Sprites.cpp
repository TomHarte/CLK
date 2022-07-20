//
//  Sprites.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 26/11/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "Sprites.hpp"

#include <cassert>

using namespace Amiga;

namespace {

/// Expands @c source from b15 ... b0 to 000b15 ... 000b0.
constexpr uint64_t expand_sprite_word(uint16_t source) {
	uint64_t result = source;
	result = (result | (result << 24)) & 0x0000'00ff'0000'00ff;
	result = (result | (result << 12)) & 0x000f'000f'000f'000f;
	result = (result | (result << 6)) & 0x0303'0303'0303'0303;
	result = (result | (result << 3)) & 0x1111'1111'1111'1111;
	return result;
}

// A very small selection of test cases.
static_assert(expand_sprite_word(0xffff) == 0x11'11'11'11'11'11'11'11);
static_assert(expand_sprite_word(0x5555) == 0x01'01'01'01'01'01'01'01);
static_assert(expand_sprite_word(0xaaaa) == 0x10'10'10'10'10'10'10'10);
static_assert(expand_sprite_word(0x0000) == 0x00'00'00'00'00'00'00'00);

}

// MARK: - Sprites.

void Sprite::set_start_position(uint16_t value) {
	// b8–b15: low 8 bits of VSTART;
	// b0–b7: high 8 bits of HSTART.
	v_start_ = (v_start_ & 0xff00) | (value >> 8);
	h_start = uint16_t((h_start & 0x0001) | ((value & 0xff) << 1));
}

void Sprite::set_stop_and_control(uint16_t value) {
	// b8–b15: low 8 bits of VSTOP;
	// b7: attachment flag;
	// b3–b6: unused;
	// b2: VSTART high bit;
	// b1: VSTOP high bit;
	// b0: HSTART low bit.
	h_start = uint16_t((h_start & 0x01fe) | (value & 0x01));
	v_stop_ = uint16_t((value >> 8) | ((value & 0x02) << 7));
	v_start_ = uint16_t((v_start_ & 0x00ff) | ((value & 0x04) << 6));
	attached = value & 0x80;

	// Disarm the sprite.
	visible = false;
}

void Sprite::set_image_data(int slot, uint16_t value) {
	// Store data; also mark sprite as visible (i.e. 'arm' it)
	// if data is being stored to slot 0.
	data[slot] = value;
	visible |= slot == 0;
}

bool Sprite::advance_dma(int offset, int y, bool is_first_line) {
	assert(offset == 0 || offset == 1);

	// Determine which word would be fetched, if DMA occurs.
	// A bit of a cheat.
	const uint16_t next_word = ram_[pointer_[0] & ram_mask_];

	// "When the vertical position of the beam counter is equal to the VSTOP
	// value in the sprite control words, the next two words fetched from the
	// sprite data structure are written into the sprite control registers
	// instead of being sent to the color registers"
	//
	// Guesswork, primarily from observing Spindizzy Worlds: the first line after
	// vertical blank also triggers a control reload. Seek to verify.
	if(y == v_stop_ || is_first_line) {
		if(offset) {
			// Second control word: stop position (mostly).
			set_stop_and_control(next_word);
		} else {
			// First control word: start position.
			set_start_position(next_word);
		}
	} else {
		visible |= y == v_start_;
		if(!visible) return false;	// Act as if there wasn't a fetch.

		// Write colour word 1, then colour word 0; 0 is the word that 'arms'
		// the sprite (i.e. makes it visible).
		set_image_data(1 - offset, next_word);
	}

	// Acknowledge the fetch.
	++pointer_[0];
	return true;
}

template <int sprite> void TwoSpriteShifter::load(
	uint16_t lsb,
	uint16_t msb,
	int delay) {
	constexpr int sprite_shift = sprite << 1;
	const int delay_shift = delay << 2;

	// Clear out any current sprite pixels; this is a reload.
	data_ &= 0xcccc'cccc'cccc'ccccull >> (sprite_shift + delay_shift);

	// Map LSB and MSB up to 64-bits and load into the shifter.
	const uint64_t new_data =
		(
			expand_sprite_word(lsb) |
			(expand_sprite_word(msb) << 1)
		) << sprite_shift;

	data_ |= new_data >> delay_shift;
	overflow_ |= uint8_t((new_data << 8) >> delay_shift);
}

template void TwoSpriteShifter::load<0>(uint16_t, uint16_t, int);
template void TwoSpriteShifter::load<1>(uint16_t, uint16_t, int);
