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
				// TODO: update state (!)

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
			if(!selected_register_) printf("Horizontal total: %d\n", value);
		}

	private:
		T &bus_handler_;
		BusState bus_state_;

		uint8_t registers_[16];
		int selected_register_;
};

}
}

#endif /* CRTC6845_hpp */
