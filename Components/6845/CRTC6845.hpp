//
//  CRTC6845.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 31/07/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#pragma once

#include "ClockReceiver/ClockReceiver.hpp"

#include <cstdint>
#include <cstdio>

namespace Motorola::CRTC {

struct BusState {
	bool display_enable = false;
	bool hsync = false;
	bool vsync = false;
	bool cursor = false;
	uint16_t refresh_address = 0;
	uint16_t row_address = 0;

	// Not strictly part of the bus state; provided because the partition between 6845 and bus handler
	// doesn't quite hold up in some emulated systems where the two are integrated and share more state.
	int field_count = 0;
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

	EGA,		// Extended EGA-style CRTC; uses 16-bit addressing throughout.
};

constexpr bool is_egavga(const Personality p) {
	return p >= Personality::EGA;
}

// https://www.pcjs.org/blog/2018/03/20/ advises that "the behavior of bits 5 and 6 [of register 10, the cursor start
// register is really card specific".
//
// This enum captures those specifics.
enum class CursorType {
	/// No cursor signal is generated.
	None,
	/// MDA style: 00 => symmetric blinking; 01 or 10 => no blinking; 11 => short on, long off.
	MDA,
	/// EGA style: ignore the bits completely.
	EGA,
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
		static constexpr bool is_ega = is_egavga(personality);

		const auto load_low = [value](uint16_t &target) {
			target = (target & 0xff00) | value;
		};
		const auto load_high = [value](uint16_t &target) {
			static constexpr uint8_t mask = RefreshMask >> 8;
			target = uint16_t((target & 0x00ff) | ((value & mask) << 8));
		};

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
				switch(value & 3) {
					default:	layout_.interlace_mode_ = InterlaceMode::Off;					break;
					case 0b01:	layout_.interlace_mode_ = InterlaceMode::InterlaceSync;			break;
					case 0b11:	layout_.interlace_mode_ = InterlaceMode::InterlaceSyncAndVideo;	break;
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
			case 9:	layout_.vertical.end_row = value & 0x1f;	break;
			case 10:
				layout_.vertical.start_cursor = value & 0x1f;
				layout_.cursor_flags = (value >> 5) & 3;
			break;
			case 11:
				layout_.vertical.end_cursor = value & 0x1f;
			break;
			case 12:	load_high(layout_.start_address);	break;
			case 13:	load_low(layout_.start_address);	break;
			case 14:	load_high(layout_.cursor_address);	break;
			case 15:	load_low(layout_.cursor_address);	break;
		}

		static constexpr uint8_t masks[] = {
			0xff,	// Horizontal total.
			0xff,	// Horizontal display end.
			0xff,	// Start horizontal blank.
			0xff,	//
					// EGA: b0–b4: end of horizontal blank;
					// b5–b6: "Number of character clocks to delay start of display after Horizontal Total has been reached."

			is_ega ? 0xff : 0x7f,	// Start horizontal retrace.
			0x1f, 0x7f, 0x7f,
			0xff, 0x1f, 0x7f, 0x1f,
			uint8_t(RefreshMask >> 8), uint8_t(RefreshMask),
			uint8_t(RefreshMask >> 8), uint8_t(RefreshMask),
		};

		if(selected_register_ < 16) {
			registers_[selected_register_] = value & masks[selected_register_];
		}
		if(selected_register_ == 31 && personality == Personality::UM6845R) {
			dummy_register_ = value;
		}
	}

	void trigger_light_pen() {
		registers_[17] = bus_state_.refresh_address & 0xff;
		registers_[16] = bus_state_.refresh_address >> 8;
		status_ |= 0x40;
	}

	void run_for(const Cycles cycles) {
		auto cyles_remaining = cycles.as_integral();
		while(cyles_remaining--) {
			// Intention of code below: all conditionals are evaluated as if functional; they should be
			// ordered so that whatever assignments result don't affect any subsequent conditionals


			// Do bus work.
			bus_state_.cursor = is_cursor_line_ &&
				bus_state_.refresh_address == layout_.cursor_address;
			bus_state_.display_enable = character_is_visible_ && line_is_visible_;
			bus_handler_.perform_bus_cycle(bus_state_);

			//
			// Shared, stateless signals.
			//
				const bool character_total_hit = character_counter_ == layout_.horizontal.total;
				const uint8_t lines_per_row =
					layout_.interlace_mode_ == InterlaceMode::InterlaceSyncAndVideo ?
						layout_.vertical.end_row & ~1 : layout_.vertical.end_row;
				const bool row_end_hit = bus_state_.row_address == lines_per_row && !is_in_adjustment_period_;
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
					if((row_counter_ == layout_.vertical.start_sync && !bus_state_.row_address) || bus_state_.vsync) {
						bus_state_.vsync = true;
						vsync_counter_ = (vsync_counter_ + 1) & 0xf;
					} else {
						vsync_counter_ = 0;
					}

					if(vsync_counter_ == layout_.vertical.sync_lines) {
						bus_state_.vsync = false;
					}
				}

				// Row address.
				if(character_total_hit) {
					if(was_eof) {
						bus_state_.row_address = 0;
						eof_latched_ = eom_latched_ = false;
					} else if(row_end_hit) {
						bus_state_.row_address = 0;
					} else if(layout_.interlace_mode_ == InterlaceMode::InterlaceSyncAndVideo) {
						bus_state_.row_address = (bus_state_.row_address + 2) & ~1 & 31;
					} else {
						bus_state_.row_address = (bus_state_.row_address + 1) & 31;
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
					is_cursor_line_ |= bus_state_.row_address == layout_.vertical.start_cursor;
					is_cursor_line_ &= bus_state_.row_address != layout_.vertical.end_cursor;

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
					bus_state_.refresh_address = layout_.start_address;
				} else if(character_total_hit) {
					bus_state_.refresh_address = line_address_;
				} else {
					bus_state_.refresh_address = (bus_state_.refresh_address + 1) & RefreshMask;
				}

				if(new_frame) {
					line_address_ = layout_.start_address;
				} else if(character_counter_ == layout_.horizontal.displayed && row_end_hit) {
					line_address_ = bus_state_.refresh_address;
				}
		}
	}

	const BusState &get_bus_state() const {
		return bus_state_;
	}

private:
	static constexpr uint16_t RefreshMask = (personality >= Personality::EGA) ? 0xffff : 0x3fff;

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
	struct {
		struct {
			uint8_t total;
			uint8_t displayed;
			uint8_t start_sync;
			uint8_t sync_width;
		} horizontal;

		struct {
			uint8_t total;
			uint8_t displayed;
			uint8_t start_sync;
			uint8_t sync_lines;
			uint8_t adjust;

			uint8_t end_row;
			uint8_t start_cursor;
			uint8_t end_cursor;
		} vertical;

		InterlaceMode interlace_mode_ = InterlaceMode::Off;
		uint8_t end_row() const {
			return interlace_mode_ == InterlaceMode::InterlaceSyncAndVideo ? vertical.end_row & ~1 : vertical.end_row;
		}

		uint16_t start_address;
		uint16_t cursor_address;
		uint16_t light_pen_address;
		uint8_t cursor_flags;
	} layout_;

	uint8_t registers_[18]{};
	uint8_t dummy_register_ = 0;
	int selected_register_ = 0;

	uint8_t character_counter_ = 0;
	uint32_t character_reset_history_ = 0;
	uint8_t row_counter_ = 0, next_row_counter_ = 0;
	uint8_t adjustment_counter_ = 0;

	bool character_is_visible_ = false;
	bool line_is_visible_ = false;
	bool is_first_scanline_ = false;
	bool is_cursor_line_ = false;

	int hsync_counter_ = 0;
	int vsync_counter_ = 0;
	bool is_in_adjustment_period_ = false;

	uint16_t line_address_ = 0;
	uint8_t status_ = 0;

	int display_skew_mask_ = 1;
	unsigned int character_is_visible_shifter_ = 0;

	bool eof_latched_ = false;
	bool eom_latched_ = false;
	uint16_t next_row_address_ = 0;
	bool odd_field_ = false;
};

}
