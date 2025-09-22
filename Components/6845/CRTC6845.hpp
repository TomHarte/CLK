//
//  CRTC6845.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 31/07/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#pragma once

#include "ClockReceiver/ClockReceiver.hpp"
#include "Numeric/SizedCounter.hpp"

#include <cstdint>
#include <cstdio>

//
// WARNING: code is in flux. I'm attempting to use hoglet's FPGA implementation at
// https://github.com/hoglet67/BeebFpga/blob/master/src/common/mc6845.vhd as an authoritative guide to proper behaviour,
// having found his Electron ULA to be excellent. This is starting by mapping various bits of internal state here
// to hoglet's equivalents; cf. comments.
//

namespace Motorola::CRTC {

using RefreshAddress = Numeric::SizedCounter<14>;
using RowAddress = Numeric::SizedCounter<7>;

using SyncCounter = Numeric::SizedCounter<4>;
using CharacterAddress = Numeric::SizedCounter<8>;
using LineAddress = Numeric::SizedCounter<5>;

struct BusState {
	bool display_enable = false;
	bool hsync = false;
	bool vsync = false;
	bool cursor = false;
	RefreshAddress refresh;	// ma_i
	LineAddress row;

	// Not strictly part of the bus state; provided because the partition between 6845 and bus handler
	// doesn't quite hold up in some emulated systems where the two are integrated and share more state.
	int field_count = 0;		// field_counter
};

class BusHandler {
public:
	void perform_bus_cycle(const BusState &) {}
};

enum class Personality {
	HD6845S,	// Type 0 in CPC parlance. Zero-width HSYNC available, no status, programmable VSYNC length.
				// Considered exactly identical to the UM6845, so this enum covers both.
	UM6845R,	// Type 1 in CPC parlance. Status register, fixed-length VSYNC.
	MC6845,		// Type 2. No status register, fixed-length VSYNC, no zero-length HSYNC.
	AMS40226,	// Type 3. Status is get register, fixed-length VSYNC, no zero-length HSYNC.
};

// https://www.pcjs.org/blog/2018/03/20/ advises that "the behavior of bits 5 and 6 [of register 10, the cursor start
// register is really card specific".
//
// This enum captures those specifics.
enum class CursorType {
	/// No cursor signal is generated.
	None,
	/// MDA style: 00 => symmetric blinking; 01 or 10 => no blinking; 11 => short on, long off.
	MDA,
};

// TODO UM6845R and R12/R13; see http://www.cpcwiki.eu/index.php/CRTC#CRTC_Differences

template <class BusHandlerT, Personality personality, CursorType cursor_type> class CRTC6845 {
public:
	CRTC6845(BusHandlerT &bus_handler) noexcept :
		bus_handler_(bus_handler), status_(0) {}

	void select_register(uint8_t r) {
		selected_register_ = r;
	}

	uint8_t get_status() {
		switch(personality) {
			case Personality::UM6845R:	return status_ | (bus_state_.vsync ? 0x20 : 0x00);
			case Personality::AMS40226:	return get_register();
			default:					return 0xff;
		}
		return 0xff;
	}

	uint8_t get_register() {
		if(selected_register_ == 31) status_ &= ~0x80;
		if(selected_register_ == 16 || selected_register_ == 17) status_ &= ~0x40;

		if(personality == Personality::UM6845R && selected_register_ == 31) return dummy_register_;
		if(selected_register_ < 12 || selected_register_ > 17) return 0xff;
		return registers_[selected_register_];
	}

	void set_register(const uint8_t value) {
		switch(selected_register_) {
			case 0:	layout_.horizontal.total = value;		break;
			case 1: layout_.horizontal.displayed = value;	break;
			case 2:	layout_.horizontal.start_sync = value;	break;
			case 3:
				layout_.horizontal.sync_width = value & 0xf;
				layout_.vertical.sync_lines = value >> 4;
				// TODO: vertical sync lines:
				// "(0 means 16 on some CRTC. Not present on all CRTCs, fixed to 16 lines on these)"
			break;
			case 4:	layout_.vertical.total = value & 0x7f;		break;
			case 5:	layout_.vertical.adjust = value & 0x1f;		break;
			case 6:	layout_.vertical.displayed = value & 0x7f;	break;
			case 7:	layout_.vertical.start_sync = value & 0x7f;	break;
			case 8:
				// TODO: an error elsewhere appears to cause modes other than InterlaceMode::Off never to hit
				// vertical sync.
//				switch(value & 3) {
//					default:	layout_.interlace_mode_ = InterlaceMode::Off;					break;
//					case 0b01:	layout_.interlace_mode_ = InterlaceMode::InterlaceSync;			break;
//					case 0b11:	layout_.interlace_mode_ = InterlaceMode::InterlaceSyncAndVideo;	break;
//				}

				// Per CPC documentation, skew doesn't work on a "type 1 or 2", i.e. an MC6845 or a UM6845R.
				if(personality != Personality::UM6845R && personality != Personality::MC6845) {
					switch((value >> 4)&3) {
						default:	display_skew_mask_ = 1;		break;
						case 1:		display_skew_mask_ = 2;		break;
						case 2:		display_skew_mask_ = 4;		break;
					}
				}
			break;
			case 9:	layout_.vertical.end_row = value & 0x1f;	break;
			case 10:
				layout_.vertical.start_cursor = value & 0x1f;
				layout_.cursor_flags = (value >> 5) & 3;
			break;
			case 11:
				layout_.vertical.end_cursor = value & 0x1f;
			break;
			case 12:	layout_.start_address.template load<8>(value);	break;
			case 13:	layout_.start_address.template load<0>(value);	break;
			case 14:	layout_.cursor_address.template load<8>(value);	break;
			case 15:	layout_.cursor_address.template load<0>(value);	break;
		}

		static constexpr uint8_t masks[] = {
			0xff,	// Horizontal total.
			0xff,	// Horizontal display end.
			0xff,	// Start horizontal blank.
			0xff,	//
					// EGA: b0–b4: end of horizontal blank;
					// b5–b6: "Number of character clocks to delay start of display after Horizontal Total has been reached."

			0x7f,	// Start horizontal retrace.
			0x1f, 0x7f, 0x7f,
			0xff, 0x1f, 0x7f, 0x1f,
			uint8_t(RefreshAddress::Mask >> 8), uint8_t(RefreshAddress::Mask),
			uint8_t(RefreshAddress::Mask >> 8), uint8_t(RefreshAddress::Mask),
		};

		if(selected_register_ < 16) {
			registers_[selected_register_] = value & masks[selected_register_];
		}
		if(selected_register_ == 31 && personality == Personality::UM6845R) {
			dummy_register_ = value;
		}
	}

	void trigger_light_pen() {
		registers_[17] = bus_state_.refresh.get() & 0xff;
		registers_[16] = bus_state_.refresh.get() >> 8;
		status_ |= 0x40;
	}

	void run_for(const Cycles cycles) {
		auto cyles_remaining = cycles.as_integral();
		while(cyles_remaining--) {
			// Intention of code below: all conditionals are evaluated as if functional; they should be
			// ordered so that whatever assignments result don't affect any subsequent conditionals


			// Do bus work.
			bus_state_.cursor = is_cursor_line_ &&
				bus_state_.refresh == layout_.cursor_address;
			bus_state_.display_enable = character_is_visible_ && line_is_visible_;
			bus_handler_.perform_bus_cycle(bus_state_);

			//
			// Shared, stateless signals.
			//
				const bool character_total_hit = character_counter_ == layout_.horizontal.total;
				const auto lines_per_row = layout_.end_row();
				const bool row_end_hit = bus_state_.row == lines_per_row && !is_in_adjustment_period_;
				const bool was_eof = eof_latched_;
				const bool new_frame =
					character_total_hit && was_eof &&
					(
						layout_.interlace_mode_ == InterlaceMode::Off ||
						!odd_field_
					);

			//
			// Horizontal.
			//

				// Update horizontal sync.
				if(bus_state_.hsync) {
					++hsync_counter_;
					bus_state_.hsync = hsync_counter_ != layout_.horizontal.sync_width;
				}
				if(character_counter_ == layout_.horizontal.start_sync) {
					hsync_counter_ = 0;
					bus_state_.hsync = true;
				}

				// Check for end-of-line.
				character_reset_history_ <<= 1;
				if(character_total_hit) {
					character_counter_ = 0;
					character_is_visible_ = true;
					character_reset_history_ |= 1;
				} else {
					character_counter_++;
				}

				// Check for end of visible characters.
				if(character_counter_ == layout_.horizontal.displayed) {
					character_is_visible_ = false;
				}

			//
			// End-of-frame.
			//

				if(character_total_hit) {
					if(was_eof) {
						eof_latched_ = eom_latched_ = is_in_adjustment_period_ = false;
						adjustment_counter_ = 0;
					} else if(is_in_adjustment_period_) {
						adjustment_counter_ = (adjustment_counter_ + 1) & 31;
					}
				}

				if(character_reset_history_ & 2) {
					eom_latched_ |= row_end_hit && row_counter_ == layout_.vertical.total;
				}

				if(character_reset_history_ & 4 && eom_latched_) {
					// TODO: I don't believe the "add 1 for interlaced" test here is accurate;
					// others represent the extra scanline as additional state, presumably because
					// adjust total might be reprogrammed at any time.
					const auto adjust_length =
						layout_.vertical.adjust + (layout_.interlace_mode_ != InterlaceMode::Off && odd_field_ ? 1 : 0);
					is_in_adjustment_period_ |= adjustment_counter_ != adjust_length;
					eof_latched_ |= adjustment_counter_ == adjust_length;
				}

			//
			// Vertical.
			//

				// Sync.
				const bool vsync_horizontal =
					(!odd_field_ && !character_counter_) ||
					(odd_field_ && character_counter_ == (layout_.horizontal.total >> 1));
				if(vsync_horizontal) {
					if((row_counter_ == layout_.vertical.start_sync && !bus_state_.row) || bus_state_.vsync) {
						bus_state_.vsync = true;
						vsync_counter_ = (vsync_counter_ + 1) & 0xf;
					} else {
						vsync_counter_ = -1;//0;		// TODO: this ensures the first time the condition above is met,
														// vsync_counter starts at 0. It's a hack though.
					}

					if(vsync_counter_ == layout_.vertical.sync_lines) {
						bus_state_.vsync = false;
					}
				}

				// Row address.
				if(character_total_hit) {
					if(was_eof) {
						bus_state_.row = 0;
						eof_latched_ = eom_latched_ = false;
					} else if(row_end_hit) {
						bus_state_.row = 0;
					} else if(layout_.interlace_mode_ == InterlaceMode::InterlaceSyncAndVideo) {
						bus_state_.row += 2;
					} else {
						++bus_state_.row;
					}
				}

				// Row counter.
				row_counter_ = next_row_counter_;
				if(new_frame) {
					next_row_counter_ = 0;
					is_first_scanline_ = true;
				} else {
					next_row_counter_ = row_end_hit && character_total_hit ?
						(next_row_counter_ + 1) : next_row_counter_;
					is_first_scanline_ &= !row_end_hit;
				}

				// Vertical display enable.
				if(is_first_scanline_) {
					line_is_visible_ = true;
					odd_field_ = bus_state_.field_count & 1;
				} else if(line_is_visible_ && row_counter_ == layout_.vertical.displayed) {
					line_is_visible_ = false;
					++bus_state_.field_count;
				}

				// Cursor.
				if constexpr (cursor_type != CursorType::None) {
					// Check for cursor enable.
					is_cursor_line_ |= bus_state_.row == layout_.vertical.start_cursor;
					is_cursor_line_ &= !(
						(bus_state_.row == layout_.vertical.end_cursor) ||
						(
							character_total_hit &&
							row_end_hit &&
							layout_.vertical.end_cursor == (lines_per_row + LineAddress(1))
						)
					);

					switch(cursor_type) {
						// MDA-style blinking.
						// https://retrocomputing.stackexchange.com/questions/27803/what-are-the-blinking-rates-of-the-caret-and-of-blinking-text-on-pc-graphics-car
						// gives an 8/8 pattern for regular blinking though mode 11 is then just a guess.
						case CursorType::MDA:
							switch(layout_.cursor_flags) {
								case 0b11: is_cursor_line_ &= (bus_state_.field_count & 8) < 3;	break;
								case 0b00: is_cursor_line_ &= bool(bus_state_.field_count & 8);	break;
								case 0b01: is_cursor_line_ = false;								break;
								case 0b10: is_cursor_line_ = true;								break;
								default: break;
							}
						break;
					}
				}

			//
			// Addressing.
			//

				if(new_frame) {
					bus_state_.refresh = layout_.start_address;
				} else if(character_total_hit) {
					bus_state_.refresh = line_address_;
				} else {
					++bus_state_.refresh;
				}

				if(new_frame) {
					line_address_ = layout_.start_address;
				} else if(character_counter_ == layout_.horizontal.displayed && row_end_hit) {
					line_address_ = bus_state_.refresh;
				}
		}
	}

	const BusState &get_bus_state() const {
		return bus_state_;
	}

private:
	BusHandlerT &bus_handler_;
	BusState bus_state_;

	enum class InterlaceMode {
		Off,
		InterlaceSync,
		InterlaceSyncAndVideo,
	};
	enum class BlinkMode {
		// TODO.
	};

	// Comments on the right provide the corresponding signal name in hoglet's VHDL implementation.
	struct {
		struct {
			CharacterAddress total;			// r00_h_total
			CharacterAddress displayed;		// r01_h_displayed
			CharacterAddress start_sync;	// r02_h_sync_pos
			SyncCounter sync_width;			// r03_h_sync_width
		} horizontal;

		struct {
			RowAddress total;			// r04_v_total
			RowAddress displayed;		// r06_v_displayed
			RowAddress start_sync;		// r07_v_sync_pos
			SyncCounter sync_lines;		// r03_v_sync_width
			LineAddress adjust;			// r05_v_total_adj

			LineAddress end_row;		// r09_max_scanline_addr
			LineAddress start_cursor;	// r10_cursor_start
			LineAddress end_cursor;		// r11_cursor_end
		} vertical;

		InterlaceMode interlace_mode_ = InterlaceMode::Off;		// r08_interlace
		LineAddress end_row() const {
			return interlace_mode_ == InterlaceMode::InterlaceSyncAndVideo ?
				vertical.end_row & ~1 : vertical.end_row;
		}

		RefreshAddress start_address;		// r12_start_addr_h + r13_start_addr_l
		RefreshAddress cursor_address;		// r14_cursor_h + r15_cursor_l
		RefreshAddress light_pen_address;	// r16_light_pen_h + r17_light_pen_l
		uint8_t cursor_flags;				// r10_cursor_mode
	} layout_;

	uint8_t registers_[18]{};
	uint8_t dummy_register_ = 0;
	int selected_register_ = 0;

	CharacterAddress character_counter_ = 0;	// h_counter
	uint32_t character_reset_history_ = 0;
	RowAddress row_counter_ = 0;				// row_counter
	RowAddress next_row_counter_ = 0;			// row_counter_next
	uint8_t adjustment_counter_ = 0;

	bool character_is_visible_ = false;			// h_display
	bool line_is_visible_ = false;				// v_display
	bool is_first_scanline_ = false;
	bool is_cursor_line_ = false;

	SyncCounter hsync_counter_;					// h_sync_counter
	SyncCounter vsync_counter_;					// v_sync_counter
	bool is_in_adjustment_period_ = false;

	RefreshAddress line_address_;
	uint8_t status_ = 0;

	int display_skew_mask_ = 1;
	unsigned int character_is_visible_shifter_ = 0;

	bool eof_latched_ = false;
	bool eom_latched_ = false;
	bool odd_field_ = false;							// odd_field

	// line_counter
	// line_counter_next
	// hs
	// vs
	// vs_hit
	// vs_hit_last
	// vs_even
	// vs_odd

};

}
