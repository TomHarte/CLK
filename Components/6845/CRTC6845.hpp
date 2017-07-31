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

namespace Motorola {

class CRTC6845 {
	public:
		void run_for(Cycles cycles) {
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
		uint8_t registers_[16];
		int selected_register_;
};

}

#endif /* CRTC6845_hpp */
