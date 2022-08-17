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

namespace {

/// @returns Either the final carry flag or the output nibble when using fill mode given that it either @c is_exclusive fill mode, or isn't;
/// and the specified initial @c carry and input @c nibble.
template <bool wants_carry> constexpr uint32_t fill_nibble(bool is_exclusive, uint8_t carry, uint8_t nibble) {
	uint8_t fill_output = 0;
	uint8_t bit = 0x01;
	while(bit < 0x10) {
		auto pre_toggle = nibble & bit, post_toggle = pre_toggle;
		if(!is_exclusive) {
			pre_toggle &= ~carry;	// Accept bits that would transition to set immediately.
			post_toggle &= carry;	// Accept bits that would transition to clear after the fact.
		} else {
			post_toggle = 0;		// Just do the pre-toggle.
		}

		carry ^= pre_toggle;
		fill_output |= carry;
		carry ^= post_toggle;

		bit <<= 1;
		carry <<= 1;
	}

	if constexpr (wants_carry) {
		return carry >> 4;
	} else {
		return fill_output;
	}
}

// Lookup key for these tables is:
//
//		b0–b3: input nibble
//		b4: carry
//		b5: is_exclusive
//
// i.e. it's in the range [0, 63].
//
// Tables below are indexed such that the higher-order bits select a table entry, lower-order bits select
// a bit or nibble from within the indexed item.

constexpr uint32_t fill_carries[] = {
	(fill_nibble<true>(false, 0, 0x0) << 0x0) | (fill_nibble<true>(false, 0, 0x1) << 0x1) | (fill_nibble<true>(false, 0, 0x2) << 0x2) | (fill_nibble<true>(false, 0, 0x3) << 0x3) |
	(fill_nibble<true>(false, 0, 0x4) << 0x4) | (fill_nibble<true>(false, 0, 0x5) << 0x5) | (fill_nibble<true>(false, 0, 0x6) << 0x6) | (fill_nibble<true>(false, 0, 0x7) << 0x7) |
	(fill_nibble<true>(false, 0, 0x8) << 0x8) | (fill_nibble<true>(false, 0, 0x9) << 0x9) | (fill_nibble<true>(false, 0, 0xa) << 0xa) | (fill_nibble<true>(false, 0, 0xb) << 0xb) |
	(fill_nibble<true>(false, 0, 0xc) << 0xc) | (fill_nibble<true>(false, 0, 0xd) << 0xd) | (fill_nibble<true>(false, 0, 0xe) << 0xe) | (fill_nibble<true>(false, 0, 0xf) << 0xf) |

	(fill_nibble<true>(false, 1, 0x0) << 0x10) | (fill_nibble<true>(false, 1, 0x1) << 0x11) | (fill_nibble<true>(false, 1, 0x2) << 0x12) | (fill_nibble<true>(false, 1, 0x3) << 0x13) |
	(fill_nibble<true>(false, 1, 0x4) << 0x14) | (fill_nibble<true>(false, 1, 0x5) << 0x15) | (fill_nibble<true>(false, 1, 0x6) << 0x16) | (fill_nibble<true>(false, 1, 0x7) << 0x17) |
	(fill_nibble<true>(false, 1, 0x8) << 0x18) | (fill_nibble<true>(false, 1, 0x9) << 0x19) | (fill_nibble<true>(false, 1, 0xa) << 0x1a) | (fill_nibble<true>(false, 1, 0xb) << 0x1b) |
	(fill_nibble<true>(false, 1, 0xc) << 0x1c) | (fill_nibble<true>(false, 1, 0xd) << 0x1d) | (fill_nibble<true>(false, 1, 0xe) << 0x1e) | (fill_nibble<true>(false, 1, 0xf) << 0x1f),

	(fill_nibble<true>(true, 0, 0x0) << 0x0) | (fill_nibble<true>(true, 0, 0x1) << 0x1) | (fill_nibble<true>(true, 0, 0x2) << 0x2) | (fill_nibble<true>(true, 0, 0x3) << 0x3) |
	(fill_nibble<true>(true, 0, 0x4) << 0x4) | (fill_nibble<true>(true, 0, 0x5) << 0x5) | (fill_nibble<true>(true, 0, 0x6) << 0x6) | (fill_nibble<true>(true, 0, 0x7) << 0x7) |
	(fill_nibble<true>(true, 0, 0x8) << 0x8) | (fill_nibble<true>(true, 0, 0x9) << 0x9) | (fill_nibble<true>(true, 0, 0xa) << 0xa) | (fill_nibble<true>(true, 0, 0xb) << 0xb) |
	(fill_nibble<true>(true, 0, 0xc) << 0xc) | (fill_nibble<true>(true, 0, 0xd) << 0xd) | (fill_nibble<true>(true, 0, 0xe) << 0xe) | (fill_nibble<true>(true, 0, 0xf) << 0xf) |

	(fill_nibble<true>(true, 1, 0x0) << 0x10) | (fill_nibble<true>(true, 1, 0x1) << 0x11) | (fill_nibble<true>(true, 1, 0x2) << 0x12) | (fill_nibble<true>(true, 1, 0x3) << 0x13) |
	(fill_nibble<true>(true, 1, 0x4) << 0x14) | (fill_nibble<true>(true, 1, 0x5) << 0x15) | (fill_nibble<true>(true, 1, 0x6) << 0x16) | (fill_nibble<true>(true, 1, 0x7) << 0x17) |
	(fill_nibble<true>(true, 1, 0x8) << 0x18) | (fill_nibble<true>(true, 1, 0x9) << 0x19) | (fill_nibble<true>(true, 1, 0xa) << 0x1a) | (fill_nibble<true>(true, 1, 0xb) << 0x1b) |
	(fill_nibble<true>(true, 1, 0xc) << 0x1c) | (fill_nibble<true>(true, 1, 0xd) << 0x1d) | (fill_nibble<true>(true, 1, 0xe) << 0x1e) | (fill_nibble<true>(true, 1, 0xf) << 0x1f),
};

constexpr uint32_t fill_values[] = {
	(fill_nibble<false>(false, 0, 0x0) << 0) | (fill_nibble<false>(false, 0, 0x1) << 4) | (fill_nibble<false>(false, 0, 0x2) << 8) | (fill_nibble<false>(false, 0, 0x3) << 12) |
	(fill_nibble<false>(false, 0, 0x4) << 16) | (fill_nibble<false>(false, 0, 0x5) << 20) | (fill_nibble<false>(false, 0, 0x6) << 24) | (fill_nibble<false>(false, 0, 0x7) << 28),

	(fill_nibble<false>(false, 0, 0x8) << 0) | (fill_nibble<false>(false, 0, 0x9) << 4) | (fill_nibble<false>(false, 0, 0xa) << 8) | (fill_nibble<false>(false, 0, 0xb) << 12) |
	(fill_nibble<false>(false, 0, 0xc) << 16) | (fill_nibble<false>(false, 0, 0xd) << 20) | (fill_nibble<false>(false, 0, 0xe) << 24) | (fill_nibble<false>(false, 0, 0xf) << 28),

	(fill_nibble<false>(false, 1, 0x0) << 0) | (fill_nibble<false>(false, 1, 0x1) << 4) | (fill_nibble<false>(false, 1, 0x2) << 8) | (fill_nibble<false>(false, 1, 0x3) << 12) |
	(fill_nibble<false>(false, 1, 0x4) << 16) | (fill_nibble<false>(false, 1, 0x5) << 20) | (fill_nibble<false>(false, 1, 0x6) << 24) | (fill_nibble<false>(false, 1, 0x7) << 28),

	(fill_nibble<false>(false, 1, 0x8) << 0) | (fill_nibble<false>(false, 1, 0x9) << 4) | (fill_nibble<false>(false, 1, 0xa) << 8) | (fill_nibble<false>(false, 1, 0xb) << 12) |
	(fill_nibble<false>(false, 1, 0xc) << 16) | (fill_nibble<false>(false, 1, 0xd) << 20) | (fill_nibble<false>(false, 1, 0xe) << 24) | (fill_nibble<false>(false, 1, 0xf) << 28),

	(fill_nibble<false>(true, 0, 0x0) << 0) | (fill_nibble<false>(true, 0, 0x1) << 4) | (fill_nibble<false>(true, 0, 0x2) << 8) | (fill_nibble<false>(true, 0, 0x3) << 12) |
	(fill_nibble<false>(true, 0, 0x4) << 16) | (fill_nibble<false>(true, 0, 0x5) << 20) | (fill_nibble<false>(true, 0, 0x6) << 24) | (fill_nibble<false>(true, 0, 0x7) << 28),

	(fill_nibble<false>(true, 0, 0x8) << 0) | (fill_nibble<false>(true, 0, 0x9) << 4) | (fill_nibble<false>(true, 0, 0xa) << 8) | (fill_nibble<false>(true, 0, 0xb) << 12) |
	(fill_nibble<false>(true, 0, 0xc) << 16) | (fill_nibble<false>(true, 0, 0xd) << 20) | (fill_nibble<false>(true, 0, 0xe) << 24) | (fill_nibble<false>(true, 0, 0xf) << 28),

	(fill_nibble<false>(true, 1, 0x0) << 0) | (fill_nibble<false>(true, 1, 0x1) << 4) | (fill_nibble<false>(true, 1, 0x2) << 8) | (fill_nibble<false>(true, 1, 0x3) << 12) |
	(fill_nibble<false>(true, 1, 0x4) << 16) | (fill_nibble<false>(true, 1, 0x5) << 20) | (fill_nibble<false>(true, 1, 0x6) << 24) | (fill_nibble<false>(true, 1, 0x7) << 28),

	(fill_nibble<false>(true, 1, 0x8) << 0) | (fill_nibble<false>(true, 1, 0x9) << 4) | (fill_nibble<false>(true, 1, 0xa) << 8) | (fill_nibble<false>(true, 1, 0xb) << 12) |
	(fill_nibble<false>(true, 1, 0xc) << 16) | (fill_nibble<false>(true, 1, 0xd) << 20) | (fill_nibble<false>(true, 1, 0xe) << 24) | (fill_nibble<false>(true, 1, 0xf) << 28),
};

}

template <bool record_bus>
void Blitter<record_bus>::set_control(int index, uint16_t value) {
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
		sequencer_.set_control(value >> 8);
	}
	shifts_[index] = value >> 12;
	LOG("Set control " << index << " to " << PADHEX(4) << value);
}

template <bool record_bus>
void Blitter<record_bus>::set_first_word_mask(uint16_t value) {
	LOG("Set first word mask: " << PADHEX(4) << value);
	a_mask_[0] = value;
}

template <bool record_bus>
void Blitter<record_bus>::set_last_word_mask(uint16_t value) {
	LOG("Set last word mask: " << PADHEX(4) << value);
	a_mask_[1] = value;
}

template <bool record_bus>
void Blitter<record_bus>::set_size(uint16_t value) {
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

template <bool record_bus>
void Blitter<record_bus>::set_minterms(uint16_t value) {
	LOG("Set minterms " << PADHEX(4) << value);
	minterms_ = value & 0xff;
}

//template <bool record_bus>
//void Blitter<record_bus>::set_vertical_size([[maybe_unused]] uint16_t value) {
//	LOG("Set vertical size " << PADHEX(4) << value);
//	// TODO. This is ECS only, I think. Ditto set_horizontal_size.
//}
//
//template <bool record_bus>
//void Blitter<record_bus>::set_horizontal_size([[maybe_unused]] uint16_t value) {
//	LOG("Set horizontal size " << PADHEX(4) << value);
//}

template <bool record_bus>
void Blitter<record_bus>::set_data(int channel, uint16_t value) {
	LOG("Set data " << channel << " to " << PADHEX(4) << value);

	// Ugh, backed myself into a corner. TODO: clean.
	switch(channel) {
		case 0: a_data_ = value; break;
		case 1: b_data_ = value; break;
		case 2: c_data_ = value; break;
		default: break;
	}
}

template <bool record_bus>
uint16_t Blitter<record_bus>::get_status() {
	const uint16_t result =
		(not_zero_flag_ ? 0x0000 : 0x2000) | (height_ ? 0x4000 : 0x0000);
	LOG("Returned status of " << result);
	return result;
}

// Due to the pipeline, writes are delayed by one slot — the first write will occur
// after the second set of inputs has been fetched, and every sequence with writes enabled
// will end with an additional write.
//
//    USE Code
//       in        Active
//    BLTCON0     Channels             Cycle Sequence
//   ---------    --------             --------------
//       F        A B C D     A0 B0 C0 -  A1 B1 C1 D0 A2 B2 C2 D1 D2
//       E        A B C       A0 B0 C0 A1 B1 C1 A2 B2 C2
//       D        A B   D     A0 B0 -  A1 B1 D0 A2 B2 D1 -  D2
//       C        A B         A0 B0 -  A1 B1 -  A2 B2
//       B        A   C D     A0 C0 -  A1 C1 D0 A2 C2 D1 -  D2
//       A        A   C       A0 C0 A1 C1 A2 C2
//       9        A     D     A0 -  A1 D0 A2 D1 -  D2
//       8        A           A0 -  A1 -  A2
//       7          B C D     B0 C0 -  -  B1 C1 D0 -  B2 C2 D1 -  D2
//       6          B C       B0 C0 -  B1 C1 -  B2 C2
//       5          B   D     B0 -  -  B1 D0 -  B2 D1 -  D2
//       4          B         B0 -  -  B1 -  -  B2
//       3            C D     C0 -  -  C1 D0 -  C2 D1 -  D2
//       2            C       C0 -  C1 -  C2
//       1              D     D0 -  D1 -  D2
//       0         none       -  -  -  -
//
//
//       Table 6-2: Typical Blitter Cycle Sequence

template <bool record_bus>
void Blitter<record_bus>::add_modulos() {
	pointer_[0] += modulos_[0] * sequencer_.channel_enabled<0>()* direction_;
	pointer_[1] += modulos_[1] * sequencer_.channel_enabled<1>() * direction_;
	pointer_[2] += modulos_[2] * sequencer_.channel_enabled<2>() * direction_;
	pointer_[3] += modulos_[3] * sequencer_.channel_enabled<3>() * direction_;
}

template <bool record_bus>
template <bool complete_immediately>
bool Blitter<record_bus>::advance_dma() {
	if(!height_) return false;

	// TODO: eliminate @c complete_immediately and this workaround.
	// See commentary in Chipset.cpp.
//	if constexpr (complete_immediately) {
//		while(get_status() & 0x4000) {
//			advance_dma<false>();
//		}
//		return true;
//	}

	if(line_mode_) {
		not_zero_flag_ = false;

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

		//
		// Caveat: I've no idea how the DMA access slots should be laid out for
		// line drawing.
		//

		if(!busy_) {
			error_ = int16_t(pointer_[0] << 1) >> 1;	// TODO: what happens if line_sign_ doesn't agree with this?
			draw_ = true;
			busy_ = true;
			has_c_data_ = false;
		}

		bool did_output = false;
		if(draw_) {
			// TODO: patterned lines. Unclear what to do with the bit that comes out of b.
			// Probably extend it to a full word?

			if(!has_c_data_) {
				has_c_data_ = true;
				c_data_ = ram_[pointer_[3] & ram_mask_];
				if constexpr (record_bus) {
					transactions_.emplace_back(Transaction::Type::ReadC, pointer_[3], c_data_);
				}
				return true;
			}

			const uint16_t output =
				apply_minterm<uint16_t>(a_data_ >> shifts_[0], b_data_, c_data_, minterms_);
			ram_[pointer_[3] & ram_mask_] = output;
			not_zero_flag_ |= output;
			draw_ &= !one_dot_;
			has_c_data_ = false;
			did_output = true;
			if constexpr (record_bus) {
				transactions_.emplace_back(Transaction::Type::WriteFromPipeline, pointer_[3], output);
			}
		}

		constexpr int LEFT	= 1 << 0;
		constexpr int RIGHT	= 1 << 1;
		constexpr int UP	= 1 << 2;
		constexpr int DOWN	= 1 << 3;
		int step = (line_direction_ & 4) ?
			((line_direction_ & 1) ? LEFT : RIGHT) :
			((line_direction_ & 1) ? UP : DOWN);

		if(error_ < 0) {
			error_ += modulos_[1];
		} else {
			step |=
				(line_direction_ & 4) ?
					((line_direction_ & 2) ? UP : DOWN) :
					((line_direction_ & 2) ? LEFT : RIGHT);

			error_ += modulos_[0];
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

		--height_;
		if(!height_) {
			busy_ = false;
			posit_interrupt(InterruptFlag::Blitter);
		}

		return did_output;
	} else {
		// Copy mode.
		if(!busy_) {
			sequencer_.begin();
			a32_ = 0;
			b32_ = 0;

			y_ = 0;
			x_ = 0;
			loop_index_ = -1;
			write_phase_ = WritePhase::Starting;
			not_zero_flag_ = false;
			busy_ = true;
		}

		const auto next = sequencer_.next();

		// If this is the start of a new iteration, check for end of line,
		// or of blit, and pick an appropriate mask for A based on location.
		if(next.second != loop_index_) {
			transient_a_mask_ = x_ ? 0xffff : a_mask_[0];

			// Check whether an entire row was completed in the previous iteration.
			// If so then add modulos. Though this won't capture the move off the
			// final line, so that's handled elsewhere.
			if(!x_ && y_) {
				add_modulos();
			}

			++x_;
			if(x_ == width_) {
				transient_a_mask_ &= a_mask_[1];
				x_ = 0;
				++y_;
				if(y_ == height_) {
					sequencer_.complete();
				}
			}
			++loop_index_;
		}

		using Channel = BlitterSequencer::Channel;
		switch(next.first) {
			case Channel::A:
				a_data_ = ram_[pointer_[0] & ram_mask_];

				if constexpr (record_bus) {
					transactions_.emplace_back(Transaction::Type::ReadA, pointer_[0], a_data_);
				}
				pointer_[0] += direction_;
			return true;
			case Channel::B:
				b_data_ = ram_[pointer_[1] & ram_mask_];

				if constexpr (record_bus) {
					transactions_.emplace_back(Transaction::Type::ReadB, pointer_[1], b_data_);
				}
				pointer_[1] += direction_;
			return true;
			case Channel::C:
				c_data_ = ram_[pointer_[2] & ram_mask_];

				if constexpr (record_bus) {
					transactions_.emplace_back(Transaction::Type::ReadC, pointer_[2], c_data_);
				}
				pointer_[2] += direction_;
			return true;
			case Channel::FlushPipeline:
				add_modulos();
				posit_interrupt(InterruptFlag::Blitter);
				height_ = 0;
				busy_ = false;

				if(write_phase_ == WritePhase::Full) {
					if constexpr (record_bus) {
						transactions_.emplace_back(Transaction::Type::WriteFromPipeline, write_address_, write_value_);
					}
					ram_[write_address_ & ram_mask_] = write_value_;
					write_phase_ = WritePhase::Starting;
				}
			return true;

			case Channel::None:
				if constexpr (record_bus) {
					transactions_.emplace_back(Transaction::Type::SkippedSlot);
				}
			return false;

			case Channel::Write:	break;
		}

		a32_ = (a32_ << 16) | (a_data_ & transient_a_mask_);
		b32_ = (b32_ << 16) | b_data_;

		uint16_t a, b;

		// The barrel shifter shifts to the right in ascending address mode,
		// but to the left otherwise.
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

		if(exclusive_fill_ || inclusive_fill_) {
			// Use the fill tables nibble-by-nibble to figure out the filled word.
			uint16_t fill_output = 0;
			int ongoing_carry = fill_carry_;
			const int type_mask = exclusive_fill_ ? (1 << 5) : 0;
			for(int c = 0; c < 16; c += 4) {
				const int total_index = (output & 0xf) | (ongoing_carry << 4) | type_mask;
				fill_output |= ((fill_values[total_index >> 3] >> ((total_index & 7) * 4)) & 0xf) << c;
				ongoing_carry = (fill_carries[total_index >> 5] >> (total_index & 31)) & 1;
				output >>= 4;
			}

			output = fill_output;
			fill_carry_ = ongoing_carry;
		}

		not_zero_flag_ |= output;

		switch(write_phase_) {
			case WritePhase::Full:
				if constexpr (record_bus) {
					transactions_.emplace_back(Transaction::Type::WriteFromPipeline, write_address_, write_value_);
				}
				ram_[write_address_ & ram_mask_] = write_value_;
				[[fallthrough]];

			case WritePhase::Starting:
				write_phase_ = WritePhase::Full;
				write_address_ = pointer_[3];
				write_value_ = output;

				if constexpr (record_bus) {
					transactions_.emplace_back(Transaction::Type::AddToPipeline, write_address_, write_value_);
				}
				pointer_[3] += direction_;
			return true;

			default: assert(false);
		}
	}

	return true;
}

template <bool record_bus>
std::vector<typename Blitter<record_bus>::Transaction> Blitter<record_bus>::get_and_reset_transactions() {
	std::vector<Transaction> result;
	std::swap(result, transactions_);
	return result;
}

template class Amiga::Blitter<false>;
template class Amiga::Blitter<true>;
template bool Amiga::Blitter<true>::advance_dma<true>();
template bool Amiga::Blitter<true>::advance_dma<false>();
template bool Amiga::Blitter<false>::advance_dma<true>();
template bool Amiga::Blitter<false>::advance_dma<false>();
