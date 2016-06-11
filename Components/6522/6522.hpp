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
		void set_register(int address, uint8_t value)
		{
			address &= 0xf;
//			printf("6522 %p: %d <- %02x\n", this, address, value);
			switch(address)
			{
				case 0x00:
					_registers.output[1] = value;
				break;
				case 0x01:
					_registers.output[0] = value;
				break;
				case 0xf:
					_registers.output[0] = value;
				break;
				case 0x02:
					_registers.data_direction[1] = value;
				break;
				case 0x03:
					_registers.data_direction[0] = value;
				break;

				// Timer 1
				case 0x06:	case 0x04:	_registers.timer_latch[0] = (_registers.timer_latch[0]&0xff00) | value;	break;
				case 0x05:	case 0x07:
					_registers.timer_latch[0] = (_registers.timer_latch[0]&0x00ff) | (uint16_t)(value << 8);
					_registers.interrupt_flags &= ~InterruptFlag::Timer1;
					if(address == 0x05)
					{
						_registers.timer[0] = _registers.timer_latch[0];
						_timer_is_running[0] = true;
					}
					reevaluate_interrupts();
				break;

				// Timer 2
				case 0x08:	_registers.timer_latch[1] = value;	break;
				case 0x09:
					_registers.interrupt_flags &= ~InterruptFlag::Timer2;
					_registers.timer[1] = _registers.timer_latch[1] | (uint16_t)(value << 8);
					_timer_is_running[1] = true;
					reevaluate_interrupts();
				break;

				// Shift
				case 0xa:	_registers.shift = value;				break;

				// Control
				case 0xb: _registers.auxiliary_control = value;		break;
				case 0xc: _registers.peripheral_control = value;	break;

				// Interrupt control
				case 0xd:
					_registers.interrupt_flags &= ~value;
					reevaluate_interrupts();
				break;
				case 0xe:
					if(value&0x80)
						_registers.interrupt_enable |= value;
					else
						_registers.interrupt_enable &= ~value;
					reevaluate_interrupts();
				break;
			}
		}

		uint8_t get_register(int address)
		{
			address &= 0xf;
//			printf("6522 %p: %d\n", this, address);
			switch(address)
			{
				case 0x00:	return _registers.input[1];
				case 0x01:	return _registers.input[0];
				case 0x02:	return _registers.data_direction[1];
				case 0x03:	return _registers.data_direction[0];
				case 0x0f:	return _registers.input[1];

				// Timer 1
				case 0x04:
					_registers.interrupt_flags &= ~InterruptFlag::Timer1;
					reevaluate_interrupts();
				return _registers.timer[0] & 0x00ff;
				case 0x05:	return _registers.timer[0] >> 8;
				case 0x06:	return _registers.timer_latch[0] & 0x00ff;
				case 0x07:	return _registers.timer_latch[0] >> 8;

				// Timer 2
				case 0x08:
					_registers.interrupt_flags &= ~InterruptFlag::Timer2;
					reevaluate_interrupts();
				return _registers.timer[1] & 0x00ff;
				case 0x09:	return _registers.timer[1] >> 8;

				case 0x0a:	return _registers.shift;

				case 0x0b:	return _registers.auxiliary_control;
				case 0x0c:	return _registers.peripheral_control;

				case 0x0d:	return _registers.interrupt_flags | (get_interrupt_line() ? 0x80 : 0x00);
				case 0x0e:	return _registers.interrupt_enable | 0x80;
			}

			return 0xff;
		}

		void run_for_cycles(unsigned int number_of_cycles)
		{
			_registers.timer[0] -= number_of_cycles;
			_registers.timer[1] -= number_of_cycles;

			if(!_registers.timer[1] && _timer_is_running[1])
			{
				_timer_is_running[1] = false;
				_registers.interrupt_flags |= InterruptFlag::Timer2;
				reevaluate_interrupts();
			}

			if(!_registers.timer[0] && _timer_is_running[0])
			{
				_registers.interrupt_flags |= InterruptFlag::Timer1;
				reevaluate_interrupts();

				// TODO: reload shouldn't occur for a further 1.5 cycles
				if(_registers.auxiliary_control&0x40)
				{
					_registers.timer[0] = _registers.timer_latch[0];
				}
				else
					_timer_is_running[0] = false;
			}
			// TODO: lots of other status effects
		}

		bool get_interrupt_line()
		{
			uint8_t interrupt_status = _registers.interrupt_flags & _registers.interrupt_enable & 0x7f;
			return !!interrupt_status;
		}

		void set_delegate(MOS6522Delegate *delegate)
		{
			_delegate = delegate;
		}

		MOS6522() :
			_timer_is_running{false, false},
			_last_posted_interrupt_status(false)
		{}

	private:
		bool _last_posted_interrupt_status;
		inline void reevaluate_interrupts()
		{
			bool new_interrupt_status = get_interrupt_line();
			if(new_interrupt_status != _last_posted_interrupt_status)
			{
				_last_posted_interrupt_status = new_interrupt_status;
				if(_delegate) _delegate->mos6522_did_change_interrupt_status(this);
			}
		}

		MOS6522Delegate *_delegate;

		struct Registers {
			uint8_t output[2], input[2], data_direction[2];
			uint16_t timer[2], timer_latch[2];
			uint8_t shift;
			uint8_t auxiliary_control, peripheral_control;
			uint8_t interrupt_flags, interrupt_enable;

			Registers() :
				data_direction{0, 0},
				interrupt_enable(0) {}
		} _registers;

		bool _timer_is_running[2];
};

}

#endif /* _522_hpp */
