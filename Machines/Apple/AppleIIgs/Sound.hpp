//
//  Sound.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/11/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#ifndef Apple_IIgs_Sound_hpp
#define Apple_IIgs_Sound_hpp

#include "../../../ClockReceiver/ClockReceiver.hpp"

namespace Apple {
namespace IIgs {
namespace Sound {

class GLU {

	public:
		void set_control(uint8_t);
		uint8_t get_control();
		void set_data(uint8_t);
		uint8_t get_data();
		void set_address_low(uint8_t);
		uint8_t get_address_low();
		void set_address_high(uint8_t);
		uint8_t get_address_high();

	private:
		uint16_t address_ = 0;
};

}
}
}

#endif /* SoundGLU_hpp */
