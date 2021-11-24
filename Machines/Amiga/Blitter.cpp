//
//  Blitter.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 22/07/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "Blitter.hpp"

#include "Minterms.hpp"

#include <cassert>

#ifndef NDEBUG
#define NDEBUG
#endif

#define LOG_PREFIX "[Blitter] "
#include "../../Outputs/Log.hpp"

using namespace Amiga;

void Blitter::set_control(int index, uint16_t value) {
	if(index) {
		line_mode_ = (value & 0x0001);
		one_dot_ = value & 0x0002;
		line_direction_ = (value >> 2) & 7;
		line_sign_ = (value & 0x0040) ? -1 : 1;

		direction_ = one_dot_ ? uint32_t(-1) : uint32_t(1);
		exclusive_fill_ = (value & 0x0010);
		inclusive_fill_ = !exclusive_fill_ && (value & 0x0008);	// Exclusive fill takes precedence. Probably? TODO: verify.
		fill_carry_ = (value & 0x0004);
	} else {
		minterms_ = value & 0xff;
		channel_enables_[3] = value & 0x100;
		channel_enables_[2] = value & 0x200;
		channel_enables_[1] = value & 0x400;
		channel_enables_[0] = value & 0x800;
	}
	shifts_[index] = value >> 12;
	LOG("Set control " << index << " to " << PADHEX(4) << value);
}

void Blitter::set_first_word_mask(uint16_t value) {
	LOG("Set first word mask: " << PADHEX(4) << value);
	a_mask_[0] = value;
}

void Blitter::set_last_word_mask(uint16_t value) {
	LOG("Set last word mask: " << PADHEX(4) << value);
	a_mask_[1] = value;
}

void Blitter::set_size(uint16_t value) {
//	width_ = (width_ & ~0x3f) | (value & 0x3f);
//	height_ = (height_ & ~0x3ff) | (value >> 6);
	width_ = value & 0x3f;
	if(!width_) width_ = 0x40;
	height_ = value >> 6;
	if(!height_) height_ = 1024;
	LOG("Set size to " << std::dec << width_ << ", " << height_);

	// Current assumption: writing this register informs the
	// blitter that it should treat itself as about to start a new line.
}

void Blitter::set_minterms(uint16_t value) {
	LOG("Set minterms " << PADHEX(4) << value);
	minterms_ = value & 0xff;
}

//void Blitter::set_vertical_size([[maybe_unused]] uint16_t value) {
//	LOG("Set vertical size " << PADHEX(4) << value);
//	// TODO. This is ECS only, I think. Ditto set_horizontal_size.
//}
//
//void Blitter::set_horizontal_size([[maybe_unused]] uint16_t value) {
//	LOG("Set horizontal size " << PADHEX(4) << value);
//}

void Blitter::set_data(int channel, uint16_t value) {
	LOG("Set data " << channel << " to " << PADHEX(4) << value);

	// Ugh, backed myself into a corner. TODO: clean.
	switch(channel) {
		case 0: a_data_ = value; break;
		case 1: b_data_ = value; break;
		case 2: c_data_ = value; break;
		default: break;
	}
}

uint16_t Blitter::get_status() {
	const uint16_t result =
		(not_zero_flag_ ? 0x0000 : 0x2000) | (height_ ? 0x4000 : 0x0000);
	LOG("Returned status of " << result);
	return result;
}

bool Blitter::advance_dma() {
	if(!height_) return false;

	not_zero_flag_ = false;
	if(line_mode_) {
		// As-yet unimplemented:
		assert(b_data_ == 0xffff);

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
		//		* bit 4 = x [=1] or y [=0] major;
		//		* bit 3 = 1 => major variable negative; otherwise positive;
		//		* bit 2 = 1 => minor variable negative; otherwise positive.

		//
		// Implementation below is heavily based on the documentation found
		// at https://github.com/niklasekstrom/blitter-subpixel-line/blob/master/Drawing%20lines%20using%20the%20Amiga%20blitter.pdf
		//

		int error = int16_t(pointer_[0] << 1) >> 1;	// TODO: what happens if line_sign_ doesn't agree with this?
		bool draw_ = true;
		while(height_--) {

			if(draw_) {
				// TODO: patterned lines. Unclear what to do with the bit that comes out of b.
				// Probably extend it to a full word?
				c_data_ = ram_[pointer_[3] & ram_mask_];
				const uint16_t output =
					apply_minterm<uint16_t>(a_data_ >> shifts_[0], b_data_, c_data_, minterms_);
				ram_[pointer_[3] & ram_mask_] = output;
				not_zero_flag_ |= output;
				draw_ &= !one_dot_;
			}

			constexpr int LEFT	= 1 << 0;
			constexpr int RIGHT	= 1 << 1;
			constexpr int UP	= 1 << 2;
			constexpr int DOWN	= 1 << 3;
			int step = (line_direction_ & 4) ?
				((line_direction_ & 1) ? LEFT : RIGHT) :
				((line_direction_ & 1) ? UP : DOWN);

			if(error < 0) {
				error += modulos_[1];
			} else {
				step |=
					(line_direction_ & 4) ?
						((line_direction_ & 2) ? UP : DOWN) :
						((line_direction_ & 2) ? LEFT : RIGHT);

				error += modulos_[0];
			}

			if(step & LEFT) {
				--shifts_[0];
				if(shifts_[0] == -1) {
					--pointer_[3];
				}
			} else if(step & RIGHT) {
				++shifts_[0];
				if(shifts_[0] == 16) {
					++pointer_[3];
				}
			}
			shifts_[0] &= 15;

			if(step & UP) {
				pointer_[3] -= modulos_[2];
				draw_ = true;
			} else if(step & DOWN) {
				pointer_[3] += modulos_[2];
				draw_ = true;
			}
		}
	} else {
		// Copy mode.

		// Quick hack: do the entire action atomically.
		a32_ = 0;
		b32_ = 0;

		for(int y = 0; y < height_; y++) {
			bool fill_carry = fill_carry_;

			for(int x = 0; x < width_; x++) {
				uint16_t a_mask = 0xffff;
				if(x == 0) a_mask &= a_mask_[0];
				if(x == width_ - 1) a_mask &= a_mask_[1];

				if(channel_enables_[0]) {
					a_data_ = ram_[pointer_[0] & ram_mask_];
					pointer_[0] += direction_;
				}
				a32_ = (a32_ << 16) | (a_data_ & a_mask);

				if(channel_enables_[1]) {
					b_data_ = ram_[pointer_[1] & ram_mask_];
					pointer_[1] += direction_;
				}
				b32_ = (b32_ << 16) | b_data_;

				if(channel_enables_[2]) {
					c_data_ = ram_[pointer_[2] & ram_mask_];
					pointer_[2] += direction_;
				}

				uint16_t a, b;

				// The barrel shifter shifts to the right in ascending address mode,
				// but to the left otherwise
				if(!one_dot_) {
					a = uint16_t(a32_ >> shifts_[0]);
					b = uint16_t(b32_ >> shifts_[1]);
				} else {
					// TODO: there must be a neater solution than this.
					a = uint16_t(
						(a32_ << shifts_[0]) |
						(a32_ >> (32 - shifts_[0]))
					);

					b = uint16_t(
						(b32_ << shifts_[1]) |
						(b32_ >> (32 - shifts_[1]))
					);
				}

				uint16_t output =
					apply_minterm<uint16_t>(
						a,
						b,
						c_data_,
						minterms_);

				// TODO: don't be so dense as below. This is the initial
				// does-it-pass-the-tests? version.
				if(exclusive_fill_ || inclusive_fill_) {
					uint16_t fill_output = 0;
					uint16_t bit = one_dot_ ? 0x0001 : 0x8000;
					uint16_t flag = fill_carry ? bit : 0x0000;
					while(bit) {
						uint16_t pre_toggle = output & bit, post_toggle = pre_toggle;
						if(inclusive_fill_) {
							pre_toggle &= ~flag;	// Accept bits that would transition to set immediately.
							post_toggle &= flag;	// Accept bits that would transition to clear after the fact.
						} else {
							post_toggle = 0;		// Just do the pre toggle.
						}

						flag ^= pre_toggle;
						fill_output |= flag;
						flag ^= post_toggle;

						fill_carry = flag;
						if(one_dot_) {
							bit <<= 1;
							flag <<= 1;
						} else {
							bit >>= 1;
							flag >>= 1;
						}
					}

					output = fill_output;
				}

				not_zero_flag_ |= output;

				if(channel_enables_[3]) {
					ram_[pointer_[3] & ram_mask_] = output;
					pointer_[3] += direction_;
				}
			}

			pointer_[0] += modulos_[0] * channel_enables_[0] * direction_;
			pointer_[1] += modulos_[1] * channel_enables_[1] * direction_;
			pointer_[2] += modulos_[2] * channel_enables_[2] * direction_;
			pointer_[3] += modulos_[3] * channel_enables_[3] * direction_;
		}
	}

	posit_interrupt(InterruptFlag::Blitter);
	height_ = 0;

	return true;
}
