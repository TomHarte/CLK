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

class MOS6522Delegate {
	public:
		virtual void mos6522_did_change_interrupt_status(void *mos6522) = 0;
};

template <class T> class MOS6522 {
	private:
		enum InterruptFlag: uint8_t {
			CA2ActiveEdge	= 1 << 0,
			CA1ActiveEdge	= 1 << 1,
			ShiftRegister	= 1 << 2,
			CB2ActiveEdge	= 1 << 3,
			CB1ActiveEdge	= 1 << 4,
			Timer2			= 1 << 5,
			Timer1			= 1 << 6,
		};

	public:
		void set_delegate(MOS6522Delegate *delegate)
		{
			_delegate = delegate;
		}

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
					_interrupt_flags &= ~InterruptFlag::Timer1;
					if(address == 0x05)
					{
						_interval_timer[0] = _interval_timer_latch[0];
						_timer_is_running[0] = true;
					}
					reevaluate_interrupts();
				break;

				// Timer 2
				case 0x08:	_interval_timer_latch[1] = value;	break;
				case 0x09:
					_interrupt_flags &= ~InterruptFlag::Timer2;
					_interval_timer[1] = _interval_timer_latch[1] | (uint16_t)(value << 8);
					_timer_is_running[1] = true;
					reevaluate_interrupts();
				break;

				case 11: _auxiliary_control_register = value;	break;
				case 13:
					if(!(value&0x80)) value = ~value;
					_interrupt_mask = value;
				break;
				case 14:
				break;
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
					_interrupt_flags &= ~InterruptFlag::Timer1;
					reevaluate_interrupts();
				return _interval_timer[0] & 0x00ff;
				case 0x05:	return _interval_timer[0] >> 8;
				case 0x06:	return _interval_timer_latch[0] & 0x00ff;
				case 0x07:	return _interval_timer_latch[0] >> 8;

				case 11:	return _auxiliary_control_register;
				case 13:	return _interrupt_flags;
				case 14:	return _interrupt_mask;
			}

			return 0xff;
		}

		void run_for_cycles(unsigned int number_of_cycles)
		{
			_interval_timer[0] -= number_of_cycles;
			_interval_timer[1] -= number_of_cycles;

			if(!_interval_timer[1] && _timer_is_running[1])
			{
				_timer_is_running[1] = false;
				_interrupt_flags |= InterruptFlag::Timer2;
				reevaluate_interrupts();
			}

			if(!_interval_timer[0] && _timer_is_running[0])
			{
				_interrupt_flags |= InterruptFlag::Timer1;
				reevaluate_interrupts();

				// TODO: reload shouldn't occur for a further 1.5 cycles
				if(_auxiliary_control_register&0x40)
				{
					_interval_timer[0] = _interval_timer_latch[0];
				}
				else
					_timer_is_running[0] = false;
			}
			// TODO: lots of other status effects
		}

		bool get_interrupt_line()
		{
			uint8_t interrupt_status = _interrupt_flags & (~_interrupt_mask) & 0x7f;
			return !!interrupt_status;
		}

	private:
		inline void reevaluate_interrupts()
		{
			if(_delegate) _delegate->mos6522_did_change_interrupt_status(this);
		}

		MOS6522Delegate *_delegate;
		uint16_t _interval_timer[2], _interval_timer_latch[2];
		uint8_t _shift_register;
		uint8_t _input_latch[2];
		uint8_t _data_direction[2];
		uint8_t _interrupt_flags, _interrupt_mask;

		bool _timer_is_running[2];

		uint8_t _interrupt_flag_register, _interrupt_enable_register;
		uint8_t _peripheral_control_register, _auxiliary_control_register;
};

}

#endif /* _522_hpp */
