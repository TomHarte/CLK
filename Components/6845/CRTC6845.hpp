//
//  CRTC6845.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 31/07/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef CRTC6845_hpp
#define CRTC6845_hpp

#include "../../ClockReceiver/ClockReceiver.hpp"

#include <cstdint>
#include <cstdio>

namespace Motorola {
namespace CRTC {

struct BusState {
	bool display_enable;
	bool hsync;
	bool vsync;
	bool cursor;
	uint16_t refresh_address;
	uint16_t row_address;
};

class BusHandler {
	public:
		/*!
			Performs the first phase of a 6845 bus cycle; this is the phase in which it is intended that
			systems using the 6845 respect the bus state and produce pixels, sync or whatever they require.
		*/
		void perform_bus_cycle_phase1(const BusState &) {}

		/*!
			Performs the second phase of a 6845 bus cycle. Some bus state — including sync — is updated
			directly after phase 1 and hence is visible to an observer during phase 2. Handlers may therefore
			implement @c perform_bus_cycle_phase2 to be notified of the availability of that state without
			having to wait until the next cycle has begun.
		*/
		void perform_bus_cycle_phase2(const BusState &) {}
};

enum Personality {
	HD6845S,	//
	UM6845R,	//
	MC6845,		//
	AMS40226	//
};

template <class T> class CRTC6845 {
	public:

		CRTC6845(Personality p, T &bus_handler) noexcept :
			personality_(p), bus_handler_(bus_handler) {}

		void select_register(uint8_t r) {
			selected_register_ = r;
		}

		uint8_t get_status() const {
			return 0xff;
		}

		uint8_t get_register() const {
			if(selected_register_ < 12 || selected_register_ > 17) return 0xff;
			return registers_[selected_register_];
		}

		void set_register(uint8_t value) {
			static uint8_t masks[] = {
				0xff, 0xff, 0xff, 0xff, 0x7f, 0x1f, 0x7f, 0x7f,
				0xff, 0x1f, 0x7f, 0x1f, 0x3f, 0xff, 0x3f, 0xff
			};

			if(selected_register_ < 16)
				registers_[selected_register_] = value & masks[selected_register_];
		}

		void trigger_light_pen() {
			registers_[17] = bus_state_.refresh_address & 0xff;
			registers_[16] = bus_state_.refresh_address >> 8;
		}

		void run_for(Cycles cycles) {
			int cyles_remaining = cycles.as_int();
			while(cyles_remaining--) {
				// check for end of visible characters
				if(character_counter_ == registers_[1]) {
					// TODO: consider skew in character_is_visible_. Or maybe defer until perform_bus_cycle?
					character_is_visible_ = false;
					end_of_line_address_ = bus_state_.refresh_address;
				}

				perform_bus_cycle_phase1();
				bus_state_.refresh_address = (bus_state_.refresh_address + 1) & 0x3fff;

				// check for end-of-line
				if(character_counter_ == registers_[0]) {
					character_counter_ = 0;
					do_end_of_line();
					character_is_visible_ = true;
				} else {
					// increment counter
					character_counter_++;
				}

				// check for start of horizontal sync
				if(character_counter_ == registers_[2]) {
					hsync_counter_ = 0;
					bus_state_.hsync = true;
				}

				// check for end of horizontal sync; note that a sync time of zero will result in an immediate
				// cancellation of the plan to perform sync
				if(bus_state_.hsync) {
					bus_state_.hsync = hsync_counter_ != (registers_[3] & 15);
					hsync_counter_ = (hsync_counter_ + 1) & 15;
				}

				perform_bus_cycle_phase2();
			}
		}

		const BusState &get_bus_state() const {
			return bus_state_;
		}

	private:
		inline void perform_bus_cycle_phase1() {
			bus_state_.display_enable = character_is_visible_ && line_is_visible_;
			bus_handler_.perform_bus_cycle_phase1(bus_state_);
		}

		inline void perform_bus_cycle_phase2() {
			bus_state_.display_enable = character_is_visible_ && line_is_visible_;
			bus_handler_.perform_bus_cycle_phase2(bus_state_);
		}

		inline void do_end_of_line() {
			// check for end of vertical sync
			if(bus_state_.vsync) {
				vsync_counter_ = (vsync_counter_ + 1) & 15;
				if(vsync_counter_ == (registers_[3] >> 4)) {
					bus_state_.vsync = false;
				}
			}

			if(is_in_adjustment_period_) {
				line_counter_++;
				if(line_counter_ == registers_[5]) {
					is_in_adjustment_period_ = false;
					do_end_of_frame();
				}
			} else {
				// advance vertical counter
				if(bus_state_.row_address == registers_[9]) {
					bus_state_.row_address = 0;
					line_address_ = end_of_line_address_;

					// check for entry into the overflow area
					if(line_counter_ == registers_[4]) {
						if(registers_[5]) {
							line_counter_ = 0;
							is_in_adjustment_period_ = true;
						} else {
							do_end_of_frame();
						}
					} else {
						line_counter_ = (line_counter_ + 1) & 0x7f;

						// check for start of vertical sync
						if(line_counter_ == registers_[7]) {
							bus_state_.vsync = true;
							vsync_counter_ = 0;
						}

						// check for end of visible lines
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
		}

		inline void do_end_of_frame() {
			line_counter_ = 0;
			line_is_visible_ = true;
			line_address_ = (uint16_t)((registers_[12] << 8) | registers_[13]);
			bus_state_.refresh_address = line_address_;
		}

		Personality personality_;
		T &bus_handler_;
		BusState bus_state_;

		uint8_t registers_[18];
		int selected_register_;

		uint8_t character_counter_;
		uint8_t line_counter_;

		bool character_is_visible_, line_is_visible_;

		int hsync_counter_;
		int vsync_counter_;
		bool is_in_adjustment_period_;

		uint16_t line_address_;
		uint16_t end_of_line_address_;
};

}
}

#endif /* CRTC6845_hpp */
