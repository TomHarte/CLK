//
//  CRTC6845.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 31/07/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef CRTC6845_hpp
#define CRTC6845_hpp

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

enum Personality {
	HD6845S,	// Type 0 in CPC parlance. Zero-width HSYNC available, no status, programmable VSYNC length.
				// Considered exactly identical to the UM6845, so this enum covers both.
	UM6845R,	// Type 1 in CPC parlance. Status register, fixed-length VSYNC.
	MC6845,		// Type 2. No status register, fixed-length VSYNC, no zero-length HSYNC.
	AMS40226	// Type 3. Status is get register, fixed-length VSYNC, no zero-length HSYNC.
};

// https://www.pcjs.org/blog/2018/03/20/ advises that "the behavior of bits 5 and 6 [of register 10, the cursor start
// register is really card specific".
//
// This enum captures those specifics.
enum CursorType {
	/// No cursor signal is generated.
	None,
	/// MDA style: 00 => symmetric blinking; 01 or 10 => no blinking; 11 => short on, long off.
	MDA,
	/// EGA style: ignore the bits completely.
	EGA,
};

// TODO UM6845R and R12/R13; see http://www.cpcwiki.eu/index.php/CRTC#CRTC_Differences

template <class T, CursorType cursor_type> class CRTC6845 {
	public:

		CRTC6845(Personality p, T &bus_handler) noexcept :
			personality_(p), bus_handler_(bus_handler), status_(0) {}

		void select_register(uint8_t r) {
			selected_register_ = r;
		}

		uint8_t get_status() {
			switch(personality_) {
				case UM6845R:	return status_ | (bus_state_.vsync ? 0x20 : 0x00);
				case AMS40226:	return get_register();
				default:		return 0xff;
			}
			return 0xff;
		}

		uint8_t get_register() {
			if(selected_register_ == 31) status_ &= ~0x80;
			if(selected_register_ == 16 || selected_register_ == 17) status_ &= ~0x40;

			if(personality_ == UM6845R && selected_register_ == 31) return dummy_register_;
			if(selected_register_ < 12 || selected_register_ > 17) return 0xff;
			return registers_[selected_register_];
		}

		void set_register(uint8_t value) {
			static constexpr uint8_t masks[] = {
				0xff, 0xff, 0xff, 0xff, 0x7f, 0x1f, 0x7f, 0x7f,
				0xff, 0x1f, 0x7f, 0x1f, 0x3f, 0xff, 0x3f, 0xff
			};

			// Per CPC documentation, skew doesn't work on a "type 1 or 2", i.e. an MC6845 or a UM6845R.
			if(selected_register_ == 8 && personality_ != UM6845R && personality_ != MC6845) {
				switch((value >> 4)&3) {
					default:	display_skew_mask_ = 1;		break;
					case 1:		display_skew_mask_ = 2;		break;
					case 2:		display_skew_mask_ = 4;		break;
				}
			}

			if(selected_register_ < 16) {
				registers_[selected_register_] = value & masks[selected_register_];
			}
			if(selected_register_ == 31 && personality_ == UM6845R) {
				dummy_register_ = value;
			}
		}

		void trigger_light_pen() {
			registers_[17] = bus_state_.refresh_address & 0xff;
			registers_[16] = bus_state_.refresh_address >> 8;
			status_ |= 0x40;
		}

		void run_for(Cycles cycles) {
			auto cyles_remaining = cycles.as_integral();
			while(cyles_remaining--) {
				// Check for end of visible characters.
				if(character_counter_ == registers_[1]) {
					// TODO: consider skew in character_is_visible_. Or maybe defer until perform_bus_cycle?
					character_is_visible_ = false;
					end_of_line_address_ = bus_state_.refresh_address;
				}

				perform_bus_cycle_phase1();
				bus_state_.refresh_address = (bus_state_.refresh_address + 1) & 0x3fff;

				bus_state_.cursor = is_cursor_line_ &&
					bus_state_.refresh_address == (registers_[15] | (registers_[14] << 8));

				// Check for end-of-line.
				if(character_counter_ == registers_[0]) {
					character_counter_ = 0;
					do_end_of_line();
					character_is_visible_ = true;
				} else {
					// Increment counter.
					character_counter_++;
				}

				// Check for start of horizontal sync.
				if(character_counter_ == registers_[2]) {
					hsync_counter_ = 0;
					bus_state_.hsync = true;
				}

				// Check for end of horizontal sync; note that a sync time of zero will result in an immediate
				// cancellation of the plan to perform sync if this is an HD6845S or UM6845R; otherwise zero
				// will end up counting as 16 as it won't be checked until after overflow.
				if(bus_state_.hsync) {
					switch(personality_) {
						case HD6845S:
						case UM6845R:
							bus_state_.hsync = hsync_counter_ != (registers_[3] & 15);
							hsync_counter_ = (hsync_counter_ + 1) & 15;
						break;
						default:
							hsync_counter_ = (hsync_counter_ + 1) & 15;
							bus_state_.hsync = hsync_counter_ != (registers_[3] & 15);
						break;
					}
				}

				perform_bus_cycle_phase2();
			}
		}

		const BusState &get_bus_state() const {
			return bus_state_;
		}

	private:
		inline void perform_bus_cycle_phase1() {
			// Skew theory of operation: keep a history of the last three states, and apply whichever is selected.
			character_is_visible_shifter_ = (character_is_visible_shifter_ << 1) | unsigned(character_is_visible_);
			bus_state_.display_enable = (int(character_is_visible_shifter_) & display_skew_mask_) && line_is_visible_;
			bus_handler_.perform_bus_cycle_phase1(bus_state_);
		}

		inline void perform_bus_cycle_phase2() {
			bus_handler_.perform_bus_cycle_phase2(bus_state_);
		}

		inline void do_end_of_line() {
			if constexpr (cursor_type != CursorType::None) {
				// Check for cursor disable.
				// TODO: this is handled differently on the EGA, should I ever implement that.
				is_cursor_line_ &= bus_state_.row_address != (registers_[11] & 0x1f);
			}

			// Check for end of vertical sync.
			if(bus_state_.vsync) {
				vsync_counter_ = (vsync_counter_ + 1) & 15;
				// On the UM6845R and AMS40226, honour the programmed vertical sync time; on the other CRTCs
				// always use a vertical sync count of 16.
				switch(personality_) {
					case HD6845S:
					case AMS40226:
						bus_state_.vsync = vsync_counter_ != (registers_[3] >> 4);
					break;
					default:
						bus_state_.vsync = vsync_counter_ != 0;
					break;
				}
			}

			if(is_in_adjustment_period_) {
				line_counter_++;
				if(line_counter_ == registers_[5]) {
					is_in_adjustment_period_ = false;
					do_end_of_frame();
				}
			} else {
				// Advance vertical counter.
				if(bus_state_.row_address == registers_[9]) {
					bus_state_.row_address = 0;
					line_address_ = end_of_line_address_;

					// Check for entry into the overflow area.
					if(line_counter_ == registers_[4]) {
						if(registers_[5]) {
							line_counter_ = 0;
							is_in_adjustment_period_ = true;
						} else {
							do_end_of_frame();
						}
					} else {
						line_counter_ = (line_counter_ + 1) & 0x7f;

						// Check for start of vertical sync.
						if(line_counter_ == registers_[7]) {
							bus_state_.vsync = true;
							vsync_counter_ = 0;
						}

						// Check for end of visible lines.
						if(line_counter_ == registers_[6]) {
							line_is_visible_ = false;
						}
					}
				} else {
					bus_state_.row_address = (bus_state_.row_address + 1) & 0x1f;
				}
			}

			bus_state_.refresh_address = line_address_;
			character_counter_ = 0;
			character_is_visible_ = (registers_[1] != 0);

			if constexpr (cursor_type != CursorType::None) {
				// Check for cursor enable.
				is_cursor_line_ |= bus_state_.row_address == (registers_[10] & 0x1f);

				switch(cursor_type) {
					// MDA-style blinking; timings are a bit of a guess for now.
					case CursorType::MDA:
						switch(registers_[10] >> 5) {
							case 0b11: is_cursor_line_ &= (field_count_ & 15) < 5;	break;
							case 0b00: is_cursor_line_ &= bool(field_count_ & 16);	break;
							case 0b01: is_cursor_line_ = false;						break;
							case 0b10: is_cursor_line_ = true;						break;
							default: break;
						}
					break;
				}
			}
		}

		inline void do_end_of_frame() {
			line_counter_ = 0;
			line_is_visible_ = true;
			line_address_ = uint16_t((registers_[12] << 8) | registers_[13]);
			bus_state_.refresh_address = line_address_;
			if constexpr (cursor_type != CursorType::None) {
				++field_count_;
			}
		}

		Personality personality_;
		T &bus_handler_;
		BusState bus_state_;

		uint8_t registers_[18] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
		uint8_t dummy_register_ = 0;
		int selected_register_ = 0;

		uint8_t character_counter_ = 0;
		uint8_t line_counter_ = 0;

		bool character_is_visible_ = false, line_is_visible_ = false;

		int hsync_counter_ = 0;
		int vsync_counter_ = 0;
		bool is_in_adjustment_period_ = false;

		uint16_t line_address_ = 0;
		uint16_t end_of_line_address_ = 0;
		uint8_t status_ = 0;

		int display_skew_mask_ = 1;
		unsigned int character_is_visible_shifter_ = 0;

		bool is_cursor_line_ = false;

		int field_count_ = 0;
};

}

#endif /* CRTC6845_hpp */
