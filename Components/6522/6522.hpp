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
		MOS6522() :
			_data_direction{0, 0},
			_timer_is_running{false, false}
		{}

		void set_register(int address, uint8_t value)
		{
			printf("6522: %d <- %02x\n", address, value);
			address &= 0xf;
			switch(address)
			{
				// Timer 1
				case 0x06:	case 0x04:	_interval_timer_latch[0] = (_interval_timer_latch[0]&0xff00) | value;	break;
				case 0x05:	case 0x07:
					_interval_timer_latch[0] = (_interval_timer_latch[0]&0x00ff) | (uint16_t)(value << 8);
					// TODO: clear interrupt flag
					if(address == 0x05)
					{
						_interval_timer[0] = _interval_timer_latch[0];
						_timer_is_running[0] = true;
					}
				break;

				// Timer 2
				case 0x08:	_interval_timer_latch[1] = value;	break;
				case 0x09:
					// TODO: clear interrupt flag
					_interval_timer[1] = _interval_timer_latch[1] | (uint16_t)(value << 8);
					_timer_is_running[1] = true;
				break;

				case 0x11: _auxiliary_control_register = value;	break;
			}
		}

		uint8_t get_register(int address)
		{
			printf("6522: %d\n", address);
			address &= 0xf;
			switch(address)
			{
				// Timer 1
				case 0x04:
					// TODO: clear interrupt flag
				return _interval_timer[0] & 0x00ff;
				case 0x05:	return _interval_timer[0] >> 8;
				case 0x06:	return _interval_timer_latch[0] & 0x00ff;
				case 0x07:	return _interval_timer_latch[0] >> 8;
			}

			return 0xff;
		}

		void run_for_cycles(unsigned int number_of_cycles)
		{
			_interval_timer[0] -= number_of_cycles;
			_interval_timer[1] -= number_of_cycles;
			// TODO: interrupts, potentially reload of the first timer, other status effects
		}

	private:
		uint16_t _interval_timer[2], _interval_timer_latch[2];
		uint8_t _shift_register;
		uint8_t _input_latch[2];
		uint8_t _data_direction[2];

		bool _timer_is_running[2];

		uint8_t _interrupt_flag_register, _interrupt_enable_register;
		uint8_t _peripheral_control_register, _auxiliary_control_register;
};

}

#endif /* _522_hpp */
