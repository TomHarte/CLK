//
//  CRTC6845.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 31/07/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#pragma once

#include "ClockReceiver/ClockReceiver.hpp"
#include "Numeric/SizedInt.hpp"

#include <cstdint>
#include <cstdio>

//
// WARNING: code is in flux. I'm attempting to use hoglet's FPGA implementation at
// https://github.com/hoglet67/BeebFpga/blob/master/src/common/mc6845.vhd as an authoritative guide to proper behaviour,
// having found his Electron ULA to be excellent. This is starting by mapping various bits of internal state here
// to hoglet's equivalents; cf. comments.
//

namespace Motorola::CRTC {

using RefreshAddress = Numeric::SizedInt<14>;
using LineAddress = Numeric::SizedInt<5>;

using SyncCounter = Numeric::SizedInt<4>;
using CharacterAddress = Numeric::SizedInt<8>;
using RowAddress = Numeric::SizedInt<7>;

struct BusState {
	bool display_enable = false;
	bool hsync = false;		// hs
	bool vsync = false;		// vs
	bool cursor = false;
	RefreshAddress refresh;
	LineAddress line;

	// Not strictly part of the bus state; provided because the partition between 6845 and bus handler
	// doesn't quite hold up in some emulated systems where the two are integrated and share more state.
	Numeric::SizedInt<5> field_count = 0;		// field_counter
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
	/// Built-in 6845 style: 00 => no blinking; 01 => no cursor; 10 => slow blink; 11 => fast blink
	Native,
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
				layout_.horizontal.sync_width = value;
				layout_.vertical.sync_lines = value >> 4;
				// TODO: vertical sync lines:
				// "(0 means 16 on some CRTC. Not present on all CRTCs, fixed to 16 lines on these)"
			break;
			case 4:	layout_.vertical.total = value;			break;
			case 5:	layout_.vertical.adjust = value;		break;
			case 6:	layout_.vertical.displayed = value;		break;
			case 7:	layout_.vertical.start_sync = value;	break;
			case 8:
				switch(value & 3) {
					default:	layout_.interlace_mode_ = InterlaceMode::Off;			break;
					case 0b01:	layout_.interlace_mode_ = InterlaceMode::Sync;			break;
					case 0b11:	layout_.interlace_mode_ = InterlaceMode::SyncAndVideo;	break;
				}

				// Per CPC documentation, skew doesn't work on a "type 1 or 2", i.e. an MC6845 or a UM6845R.
				if(personality != Personality::UM6845R && personality != Personality::MC6845) {
					switch((value >> 4)&3) {
						default:	display_skew_mask_ = 1;		break;
						case 1:		display_skew_mask_ = 2;		break;
						case 2:		display_skew_mask_ = 4;		break;
					}
				}
			break;
			case 9:	layout_.vertical.end_line = value;	break;
			case 10:
				layout_.vertical.start_cursor = value;
				layout_.cursor_flags = value >> 5;
				update_cursor_mask();
			break;
			case 11:
				layout_.vertical.end_cursor = value;
			break;
			case 12:	layout_.start_address.template load<8>(value);	break;
			case 13:	layout_.start_address.template load<0>(value);	break;
			case 14:	layout_.cursor_address.template load<8>(value);	break;
			case 15:	layout_.cursor_address.template load<0>(value);	break;
		}

		// Take redundant copies of all registers, limited to their actual bit sizes,
		// to proffer up if the registers are read.
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


			//
			// External bus activity.
			//
				bus_state_.line = line_is_interlaced_ ? (line_ & LineAddress::IntT(~1)) | (odd_field_ ? 1 : 0) : line_;
				bus_state_.display_enable = character_is_visible_ && row_is_visible_;
				bus_state_.cursor = (cursor_mask_ && is_cursor_line_ && bus_state_.refresh == layout_.cursor_address)
					&& bus_state_.display_enable;

				bus_handler_.perform_bus_cycle(bus_state_);

				bus_state_.refresh = refresh_;	// Deliberate: do this after bus activity.
												// TODO: is this a hack?


			//
			// Shared signals.
			//
				const bool character_total_hit = character_counter_ == layout_.horizontal.total;		// r00_h_total_hit
				const auto lines_per_row =
					layout_.interlace_mode_ == InterlaceMode::SyncAndVideo ?
						layout_.vertical.end_line & LineAddress::IntT(~1) : layout_.vertical.end_line;	// max_scanline
				const bool line_end_hit = line_ == lines_per_row && !is_in_adjustment_period_;	// max_scanline_hit
				const bool new_frame =
					character_total_hit && eof_latched_ &&
					(
						layout_.interlace_mode_ == InterlaceMode::Off ||
						!bus_state_.field_count.bit<0>() ||
						extra_line_
					);		// new_frame


			//
			// Addressing.
			//

				const auto initial_line_address = line_address_;
				if(new_frame) {
					line_address_ = layout_.start_address;
				} else if(character_counter_ == layout_.horizontal.displayed && line_end_hit) {
					line_address_ = refresh_;
				}

				if(new_frame) {
					refresh_ = layout_.start_address;
				} else if(character_total_hit) {
					refresh_ = initial_line_address;
				} else {
					++refresh_;
				}

				// Follow hoglet's lead in means of avoiding the logic that informs line address b0 varying
				// within a line if interlace mode is enabled/disabled.
				if(character_total_hit) {
					line_is_interlaced_ = layout_.interlace_mode_ == InterlaceMode::SyncAndVideo;
				}

			//
			// Sync.
			//

				// Vertical sync.
				//
				// Counter:
				// Sync width of 0 => 16 lines of sync.
				// Triggered by the row counter becoming equal to the sync start position, regardless of when.
				// Subsequently increments at the start of each line.
				const bool hit_vsync = row_counter_ == layout_.vertical.start_sync;		// vs_hit
				const bool is_vsync_rising_edge = hit_vsync && !hit_vsync_last_;
				hit_vsync_last_ = hit_vsync;

				// Select odd or even sync depending on the field.
				// (Noted: the reverse-odd-test is intentional)
				bus_state_.vsync = (layout_.interlace_mode_ != InterlaceMode::Off && !odd_field_) ?
					vsync_odd_ : vsync_even_;

				// Odd sync copies even sync, but half a line later.
				if(character_counter_ == layout_.horizontal.total >> 1) {
					vsync_odd_ = vsync_even_;
				}

				// Even sync begins on the rising edge of vsync, then continues until the counter hits its proper
				// target, one cycle after reset of the horizontal counter.
				if(is_vsync_rising_edge) {
					vsync_even_ = true;
				} else if(vsync_counter_ == layout_.vertical.sync_lines && character_reset_history_.bit<0>()) {
					vsync_even_ = false;
				}

				// The vsync counter is zeroed by the rising edge of sync but subsequently increments immediately
				// upon reset of the horizontal counter.
				if(is_vsync_rising_edge) {
					vsync_counter_ = 0;
				} else if(character_total_hit) {
					++vsync_counter_;
				}

				// Horizontal sync.
				//
				// A sync width of 0 should mean that no sync is observed.
				// Hitting the start sync condition while sync is already ongoing should have no effect.
				if(bus_state_.hsync) {
					++hsync_counter_;
				} else {
					hsync_counter_ = 0;
				}
				if(hsync_counter_ == layout_.horizontal.sync_width) {
					bus_state_.hsync = false;
				} else if(character_counter_ == layout_.horizontal.start_sync) {
					bus_state_.hsync = true;
				}


			//
			// Horizontal.
			//

				// Check for visible characters; visibility starts in the first column and continues
				if(!character_counter_) {
					character_is_visible_ = true;
				}
				if(character_counter_ == layout_.horizontal.displayed || character_total_hit) {
					character_is_visible_ = false;
				}

				// Check for end-of-line.
				//
				// character_reset_history_ is used because some events are defined to occur one or two
				// cycles after end-of-line regardless of whether an additional end of line is hit in
				// the interim.
				if(character_total_hit) {
					character_counter_ = 0;
				} else {
					++character_counter_;
				}


			//
			// Vertical.
			//

				// Update line counter (which also counts the vertical adjust period).
				//
				// Counts in steps of 2 only if & 3) mode is InterlaceMode::SyncAndVideo and this is
				// not the adjustment period. Otherwise counts in steps of 1.
				if(new_frame) {
					line_ = 0;
				} else if(character_total_hit) {
					line_ = next_line_;
				}

				if(line_end_hit) {
					next_line_ = 0;
				} else if(is_in_adjustment_period_ || layout_.interlace_mode_ != InterlaceMode::SyncAndVideo) {
					next_line_ = line_ + 1;
				} else {
					next_line_ = (line_ + 2) & LineAddress::IntT(~1);
				}

				// Update row counter.
				//
				// Very straightforward: tests at end of line whether row end has also been hit. If so, increments.
				row_counter_ = next_row_counter_;
				if(new_frame) {
					next_row_counter_ = 0;
				} else if(character_total_hit && line_end_hit) {
					next_row_counter_ = row_counter_ + 1;
				}

				// Vertical display enable.
				if(is_first_scanline_) {
					row_is_visible_ = true;
					odd_field_ = bus_state_.field_count.bit<0>();
				} else if(row_is_visible_ && row_counter_ == layout_.vertical.displayed) {
					row_is_visible_ = false;
					++bus_state_.field_count;
					update_cursor_mask();
				}


			//
			// End-of-frame.
			//

				if(new_frame) {
					is_in_adjustment_period_ = false;
				} else if(character_total_hit && eom_latched_ && will_adjust_) {
					is_in_adjustment_period_ = true;
				}

				if(new_frame) {
					is_first_scanline_ = true;
				} else if(character_total_hit) {
					is_first_scanline_ = false;
				}

				if(
					character_total_hit &&
					eof_latched_ &&
					layout_.interlace_mode_ != InterlaceMode::Off &&
					bus_state_.field_count.bit<0>() &&
					!extra_line_
				) {
					extra_line_ = true;
				} else if(character_total_hit) {
					extra_line_ = false;
				}

				if(new_frame) {
					eof_latched_ = false;
				} else if(eom_latched_ && !will_adjust_ && character_reset_history_.bit<2>()) {
					eof_latched_ = true;
				}

				if(new_frame) {
					will_adjust_ = false;
				} else if(character_reset_history_.bit<1>() && eom_latched_) {
					if(next_line_ == layout_.vertical.adjust) {
						will_adjust_ = false;
					} else {
						will_adjust_ = true;
					}
				}

				// EOM (end of main) marks the end of the visible set of rows, prior to any adjustment area. So it
				if(new_frame) {
					eom_latched_ = false;
				} else if(character_reset_history_.bit<0>() && line_end_hit && row_counter_ == layout_.vertical.total) {
					eom_latched_ = true;
				}

			//
			// Cursor
			//
				cursor_history_ <<= 1;
				if constexpr (cursor_type != CursorType::None) {
					// Check for cursor enable.

					is_cursor_line_ |= line_ == layout_.vertical.start_cursor;
					is_cursor_line_ &= !(
						(line_ == layout_.vertical.end_cursor) ||
						(
							character_total_hit &&
							line_end_hit &&
							layout_.vertical.end_cursor == (lines_per_row + LineAddress(1))
						)
					);
					// TODO: the above is clearly a quick-fix hack. Research better.

				}


			//
			// Event history.
			//

				// Somewhat of a fiction, this keeps a track of recent character resets because
				// some events are keyed on 1 cycle after last reset, 2 cycles after last reset, etc.
				character_reset_history_ <<= 1;
				character_reset_history_ |= character_total_hit;
		}
	}

	const BusState &get_bus_state() const {
		return bus_state_;
	}

private:
	BusHandlerT &bus_handler_;
	BusState bus_state_;

	enum class InterlaceMode {
		/// No interlacing.
		Off,
		/// Provide interlaced sync, but just scan out the exact same display for each field.
		Sync,
		/// Provide interlaced sync and scan even/odd lines depending on field.
		SyncAndVideo,
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

			LineAddress end_line;		// r09_max_scanline_addr
			LineAddress start_cursor;	// r10_cursor_start
			LineAddress end_cursor;		// r11_cursor_end
		} vertical;

		InterlaceMode interlace_mode_ = InterlaceMode::Off;		// r08_interlace

		RefreshAddress start_address;		// r12_start_addr_h + r13_start_addr_l
		RefreshAddress cursor_address;		// r14_cursor_h + r15_cursor_l
		RefreshAddress light_pen_address;	// r16_light_pen_h + r17_light_pen_l
		Numeric::SizedInt<2> cursor_flags;	// r10_cursor_mode
	} layout_;

	uint8_t registers_[18]{};
	uint8_t dummy_register_ = 0;
	int selected_register_ = 0;

	CharacterAddress character_counter_;		// h_counter
	Numeric::SizedInt<3> character_reset_history_;	// sol
	RowAddress row_counter_;					// row_counter
	RowAddress next_row_counter_;				// row_counter_next
	LineAddress line_;							// line_counter
	LineAddress next_line_;						// line_counter_next
	RefreshAddress refresh_;					// ma_i
	uint8_t adjustment_counter_ = 0;

	bool character_is_visible_ = false;			// h_display
	bool row_is_visible_ = false;				// v_display
	bool is_first_scanline_ = false;
	bool is_cursor_line_ = false;
	bool cursor_mask_ = false;

	SyncCounter hsync_counter_;					// h_sync_counter
	SyncCounter vsync_counter_;					// v_sync_counter
	bool will_adjust_ = false;					// in_adj
	bool is_in_adjustment_period_ = false;		// adj_in_progress

	RefreshAddress line_address_;				// ma_row
	uint8_t status_ = 0;

	int display_skew_mask_ = 1;
	unsigned int character_is_visible_shifter_ = 0;

	bool eof_latched_ = false;					// eof_latched
	bool eom_latched_ = false;					// eom_latched
	bool odd_field_ = false;					// odd_field
	bool extra_line_ = false;					// extra_scanline

	bool hit_vsync_last_ = false;				// vs_hit_last
	bool vsync_even_ = false;					// vs_even
	bool vsync_odd_ = false;					// vs_odd

	bool reset_ = false;

	Numeric::SizedInt<3> cursor_history_;	// cursor0, cursor1, cursor2 [TODO]
	bool line_is_interlaced_ = false;

	void update_cursor_mask() {
		switch(cursor_type) {
			case CursorType::None:
			break;

			// MDA-style blinking.
			// https://retrocomputing.stackexchange.com/questions/27803/what-are-the-blinking-rates-of-the-caret-and-of-blinking-text-on-pc-graphics-car
			// gives an 8/8 pattern for regular blinking though mode 11 is then just a guess.
			case CursorType::MDA:
				switch(layout_.cursor_flags.get()) {
					case 0b11: cursor_mask_ = (bus_state_.field_count & 8) < 3;	break;
					case 0b00: cursor_mask_ = bus_state_.field_count.bit<3>();	break;
					case 0b01: cursor_mask_ = false;							break;
					case 0b10: cursor_mask_ = true;								break;
					default: break;
				}
			break;

			// Standard built-in 6845 blinking.
			case CursorType::Native:
				switch(layout_.cursor_flags.get()) {
					case 0b00: cursor_mask_ = true;								break;
					case 0b01: cursor_mask_ = false;							break;
					case 0b10: cursor_mask_ = bus_state_.field_count.bit<4>();	break;
					case 0b11: cursor_mask_= bus_state_.field_count.bit<3>();	break;
					default: break;
				}
			break;
		}
	}
};

}
