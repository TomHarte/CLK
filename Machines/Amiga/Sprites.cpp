//
//  Sprites.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 26/11/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "Sprites.hpp"

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
	v_start_ = (v_start_ & 0xff00) | (value >> 8);
	h_start = uint16_t((h_start & 0x0001) | ((value & 0xff) << 1));
}

void Sprite::set_stop_and_control(uint16_t value) {
	h_start = uint16_t((h_start & 0x01fe) | (value & 0x01));
	v_stop_ = uint16_t((value >> 8) | ((value & 0x02) << 7));
	v_start_ = uint16_t((v_start_ & 0x00ff) | ((value & 0x04) << 6));
	attached = value & 0x80;

	// Disarm the sprite, but expect graphics next from DMA.
	visible = false;
	dma_state_ = DMAState::FetchImage;
}

void Sprite::set_image_data(int slot, uint16_t value) {
	data[slot] = value;
	visible |= slot == 0;
}

void Sprite::advance_line(int y, bool is_end_of_blank) {
	if(dma_state_ == DMAState::FetchImage && y == v_start_) {
		visible = true;
	}
	if(is_end_of_blank || y == v_stop_) {
		dma_state_ = DMAState::FetchControl;
		visible = true;
	}
}

bool Sprite::advance_dma(int offset) {
	if(!visible) return false;

	// Fetch another word.
	const uint16_t next_word = ram_[pointer_[0] & ram_mask_];
	++pointer_[0];

	// Put the fetched word somewhere appropriate and update the DMA state.
	switch(dma_state_) {
		// i.e. stopped.
		default: return false;

		case DMAState::FetchControl:
			if(offset) {
				set_stop_and_control(next_word);
			} else {
				set_start_position(next_word);
			}
		return true;

		case DMAState::FetchImage:
			set_image_data(1 - bool(offset), next_word);
		return true;
	}
	return false;
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
