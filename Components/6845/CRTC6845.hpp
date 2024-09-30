//
//  CRTC6845.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 31/07/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#pragma once

#include "../../ClockReceiver/ClockReceiver.hpp"

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
		/*!
			Performs the first phase of a 6845 bus cycle; this is the phase in which it is intended that
			systems using the 6845 respect the bus state and produce pixels, sync or whatever they require.
		*/
		void perform_bus_cycle_phase1(const BusState &) {}

		/*!
			Performs the second phase of a 6845 bus cycle. Some bus state, including sync, is updated
			directly after phase 1 and hence is visible to an observer during phase 2. Handlers may therefore
			implement @c perform_bus_cycle_phase2 to be notified of the availability of that state without
			having to wait until the next cycle has begun.
		*/
		void perform_bus_cycle_phase2(const BusState &) {}
};

enum class Personality {
	HD6845S,	// Type 0 in CPC parlance. Zero-width HSYNC available, no status, programmable VSYNC length.
				// Considered exactly identical to the UM6845, so this enum covers both.
	UM6845R,	// Type 1 in CPC parlance. Status register, fixed-length VSYNC.
	MC6845,		// Type 2. No status register, fixed-length VSYNC, no zero-length HSYNC.
	AMS40226,	// Type 3. Status is get register, fixed-length VSYNC, no zero-length HSYNC.

	EGA,		// Extended EGA-style CRTC; uses 16-bit addressing throughout.
};

constexpr bool is_egavga(Personality p) {
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

		void set_register(uint8_t value) {
			static constexpr bool is_ega = is_egavga(personality);

			auto load_low = [value](uint16_t &target) {
				target = (target & 0xff00) | value;
			};
			auto load_high = [value](uint16_t &target) {
				constexpr uint8_t mask = RefreshMask >> 8;
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

		bool eof_latched_ = false;
		bool eom_latched_ = false;
		bool extra_scanline_ = false;
		uint16_t next_row_address_ = 0;
		bool adjustment_in_progress_ = false;
		bool odd_field_ = false;

		void run_for(Cycles cycles) {
			auto cyles_remaining = cycles.as_integral();
			while(cyles_remaining--) {
				// Intention of code below: all conditionals are evaluated as if functional; they should be
				// ordered so that whatever assignments result don't affect any subsequent conditionals


				// Do bus work.
//				bus_state_.cursor = is_cursor_line_ &&
//					bus_state_.refresh_address == layout_.cursor_address;

				bus_state_.display_enable = character_is_visible_ && line_is_visible_;

				// TODO: considate the two below.
				perform_bus_cycle_phase1();
				perform_bus_cycle_phase2();

				//
				const bool character_total_hit = character_counter_ == layout_.horizontal.total;
				const uint8_t lines_per_row = layout_.interlace_mode_ == InterlaceMode::InterlaceSyncAndVideo ? layout_.vertical.end_row & ~1 : layout_.vertical.end_row;
				const bool row_end_hit = bus_state_.row_address == lines_per_row && !adjustment_in_progress_;
				const bool new_frame =
					character_total_hit && eof_latched_ &&
					(
						layout_.interlace_mode_ == InterlaceMode::Off ||
						!(bus_state_.field_count&1) ||
						extra_scanline_
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
					if(character_total_hit) {
						character_counter_ = 0;
						character_is_visible_ = true;
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

					if(new_frame) {
						eom_latched_ = eof_latched_ = false;
					} else {
						eom_latched_ |= character_counter_ == 1 && row_end_hit && row_counter_ == layout_.vertical.total;
						eof_latched_ |= character_counter_ == 2 && eom_latched_ && !adjustment_in_progress_;
					}

				//
				// Vertical.
				//

					if(character_total_hit) {
						if(eof_latched_ ) {
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

					row_counter_ = next_row_counter_;
					if(new_frame) {
						next_row_counter_ = 0;
						is_first_scanline_ = true;
					} else {
						next_row_counter_ = row_end_hit && character_total_hit ? (next_row_counter_ + 1) : next_row_counter_;
						is_first_scanline_ &= !row_end_hit;
					}

					// Sync.
					const bool vsync_horizontal =
						(!odd_field_ && !character_counter_) ||
						(odd_field_ && character_counter_ == (layout_.horizontal.total >> 1));
					if(vsync_horizontal) {
						if(row_counter_ == layout_.vertical.start_sync || bus_state_.vsync) {
							bus_state_.vsync = true;
							vsync_counter_ = (vsync_counter_ + 1) & 0xf;
						} else {
							vsync_counter_ = 0;
						}

						if(vsync_counter_ == layout_.vertical.sync_lines) {
							bus_state_.vsync = false;
						}
					}

					if(is_first_scanline_) {
						line_is_visible_ = true;
					} else if(line_is_visible_ && row_counter_ == layout_.vertical.displayed) {
						line_is_visible_ = false;
//						++field_counter_;
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

		inline void perform_bus_cycle_phase1() {
			// Skew theory of operation: keep a history of the last three states, and apply whichever is selected.
//			character_is_visible_shifter_ = (character_is_visible_shifter_ << 1) | unsigned(character_is_visible_);
//			bus_state_.display_enable = (int(character_is_visible_shifter_) & display_skew_mask_) && line_is_visible_;
			bus_handler_.perform_bus_cycle_phase1(bus_state_);
		}

		inline void perform_bus_cycle_phase2() {
			bus_handler_.perform_bus_cycle_phase2(bus_state_);
		}

//		inline void do_end_of_line() {
//			if constexpr (cursor_type != CursorType::None) {
//				// Check for cursor disable.
//				// TODO: this is handled differently on the EGA, should I ever implement that.
//				is_cursor_line_ &= bus_state_.row_address != layout_.vertical.end_cursor;
//			}
//
//			// Check for end of vertical sync.
//			if(bus_state_.vsync) {
//				vsync_counter_ = (vsync_counter_ + 1) & 15;
//				// On the UM6845R and AMS40226, honour the programmed vertical sync time; on the other CRTCs
//				// always use a vertical sync count of 16.
//				switch(personality) {
//					case Personality::HD6845S:
//					case Personality::AMS40226:
//						bus_state_.vsync = vsync_counter_ != layout_.vertical.sync_lines;
//					break;
//					default:
//						bus_state_.vsync = vsync_counter_ != 0;
//					break;
//				}
//			}
//
//			if(is_in_adjustment_period_) {
//				line_counter_++;
//				if(line_counter_ == layout_.vertical.adjust) {
//					is_in_adjustment_period_ = false;
//					do_end_of_frame();
//				}
//			} else {
//				// Advance vertical counter.
//				if(bus_state_.row_address == layout_.end_row()) {
//					bus_state_.row_address = 0;
//					line_address_ = end_of_line_address_;
//
//					// Check for entry into the overflow area.
//					if(line_counter_ == layout_.vertical.total) {
//						if(layout_.vertical.adjust) {
//							line_counter_ = 0;
//							is_in_adjustment_period_ = true;
//						} else {
//							do_end_of_frame();
//						}
//					} else {
//						line_counter_ = (line_counter_ + 1) & 0x7f;
//					}
//
//					// Check for start of vertical sync.
//					if(line_counter_ == layout_.vertical.start_sync) {
//						bus_state_.vsync = true;
//						vsync_counter_ = 0;
//					}
//
//					// Check for end of visible lines.
//					if(line_counter_ == layout_.vertical.displayed) {
//						line_is_visible_ = false;
//					}
//				} else {
//					bus_state_.row_address = (bus_state_.row_address + 1) & 0x1f;
//				}
//			}
//
//			bus_state_.refresh_address = line_address_;
//			character_counter_ = 0;
//			character_is_visible_ = (layout_.horizontal.displayed != 0);
//
//			if constexpr (cursor_type != CursorType::None) {
//				// Check for cursor enable.
//				is_cursor_line_ |= bus_state_.row_address == layout_.vertical.start_cursor;
//
//				switch(cursor_type) {
//					// MDA-style blinking.
//					// https://retrocomputing.stackexchange.com/questions/27803/what-are-the-blinking-rates-of-the-caret-and-of-blinking-text-on-pc-graphics-car
//					// gives an 8/8 pattern for regular blinking though mode 11 is then just a guess.
//					case CursorType::MDA:
//						switch(layout_.cursor_flags) {
//							case 0b11: is_cursor_line_ &= (bus_state_.field_count & 8) < 3;	break;
//							case 0b00: is_cursor_line_ &= bool(bus_state_.field_count & 8);	break;
//							case 0b01: is_cursor_line_ = false;								break;
//							case 0b10: is_cursor_line_ = true;								break;
//							default: break;
//						}
//					break;
//				}
//			}
//		}
//
//		inline void do_end_of_frame() {
//			line_counter_ = 0;
//			line_is_visible_ = true;
//			line_address_ = layout_.start_address;
//			bus_state_.refresh_address = line_address_;
//			++bus_state_.field_count;
//		}

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
		uint8_t row_counter_ = 0, next_row_counter_ = 0;;

		bool character_is_visible_ = false, line_is_visible_ = false, is_first_scanline_ = false;

		int hsync_counter_ = 0;
		int vsync_counter_ = 0;
		bool is_in_adjustment_period_ = false;

		uint16_t line_address_ = 0;
		uint16_t end_of_line_address_ = 0;
		uint8_t status_ = 0;

		int display_skew_mask_ = 1;
		unsigned int character_is_visible_shifter_ = 0;

		bool is_cursor_line_ = false;
};

}
