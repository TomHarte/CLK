//
//  6522.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/06/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef _522_hpp
#define _522_hpp

#include <cstdint>
#include <cstdio>

namespace MOS {

template <class T> class MOS6522 {
	public:
		MOS6522() : _data_direction{0, 0} {}

		void set_register(int address, uint8_t value)
		{
			printf("6522: %d <- %02x\n", address, value);
//			switch(address)
//			{
//				case 0x11: _auxiliary_control_register = value;	break;
//			}
		}
		uint8_t get_register(int address)
		{
			printf("6522: %d\n", address);
			return 0xff;
		}

	private:
		uint16_t _interval_timers[2];
		uint8_t _shift_register;
		uint8_t _input_latches[2];
		uint8_t _data_direction[2];

		uint8_t _interrupt_flag_register, _interrupt_enable_register;
		uint8_t _peripheral_control_register, _auxiliary_control_register;
};

}

#endif /* _522_hpp */
