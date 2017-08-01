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

template <class T> class CRTC6845 {
	public:
		CRTC6845(T &bus_handler) : bus_handler_(bus_handler) {}

		void run_for(Cycles cycles) {
			int cyles_remaining = cycles.as_int();
			while(cyles_remaining--) {
				// check for end of horizontal sync
				if(hsync_down_counter_) {
					hsync_down_counter_--;
					if(!hsync_down_counter_) {
						bus_state_.hsync = false;
					}
				}

				// check for start of horizontal sync
				if(character_counter_ == registers_[2]) {
					hsync_down_counter_ = registers_[3] & 15;
					if(hsync_down_counter_) bus_state_.hsync = true;
				}

				// check for end of visible characters
				if(character_counter_ == registers_[1]) {
					character_is_visible_ = false;
				}

				// update refresh address
				if(character_is_visible_) {
					bus_state_.refresh_address++;
				}

				// check for end-of-line
				if(character_counter_ == registers_[0]) {
					character_counter_ = 0;
					character_is_visible_ = true;

					// check for end of vertical sync
					if(vsync_down_counter_) {
						vsync_down_counter_--;
						if(!vsync_down_counter_) {
							bus_state_.vsync = false;
						}
					}

					if(is_in_adjustment_period_) {
						line_counter_++;
						if(line_counter_ == registers_[5]) {
							line_counter_ = 0;
							is_in_adjustment_period_ = false;
							line_is_visible_ = true;
							line_address_ = (uint16_t)((registers_[12] << 8) | registers_[13]);
							bus_state_.refresh_address = line_address_;
						}
					} else {
						// advance vertical counter
						if(bus_state_.row_address == registers_[9]) {
							line_address_ = bus_state_.refresh_address;
							bus_state_.row_address = 0;
							line_counter_++;

							// check for end of visible lines
							if(line_counter_ == registers_[6]) {
								line_is_visible_ = false;
							}

							// check for start of vertical sync
							if(line_counter_ == registers_[7]) {
								bus_state_.vsync = true;
								vsync_down_counter_ = 16;	// TODO
							}

							// check for entry into the overflow area
							if(line_counter_ == registers_[4]) {
								if(registers_[5]) {
									is_in_adjustment_period_ = true;
								} else {
									line_is_visible_ = true;
									line_address_ = (uint16_t)((registers_[12] << 8) | registers_[13]);
									bus_state_.refresh_address = line_address_;
								}
								bus_state_.row_address = 0;
								line_counter_ = 0;
							}
						} else {
							bus_state_.row_address++;
							bus_state_.refresh_address = line_address_;
						}
					}
				} else {
					// advance horizontal counter
					character_counter_++;
				}

				bus_state_.display_enable = character_is_visible_ && line_is_visible_;
				bus_handler_.perform_bus_cycle(bus_state_);
			}
		}

		void select_register(uint8_t r) {
			selected_register_ = (int)r & 15;
		}

		uint8_t get_status() {
			return 0xff;
		}

		uint8_t get_register() {
			return registers_[selected_register_];
		}

		void set_register(uint8_t value) {
			registers_[selected_register_] = value;
		}

	private:
		T &bus_handler_;
		BusState bus_state_;

		uint8_t registers_[16];
		int selected_register_;

		uint8_t character_counter_;
		uint8_t line_counter_;

		bool character_is_visible_, line_is_visible_;

		int hsync_down_counter_;
		int vsync_down_counter_;
		bool is_in_adjustment_period_;
		uint16_t line_address_;
};

}
}

#endif /* CRTC6845_hpp */
