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

/*!
	Implements a template for emulation of the MOS 6522 Versatile Interface Adaptor ('VIA').

	The VIA provides:
		* two timers, each of which may trigger interrupts and one of which may repeat;
		* two digial input/output ports; and
		* a serial-to-parallel shifter.

	Consumers should derive their own curiously-recurring-template-pattern subclass,
	implementing bus communications as required.
*/
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
		enum Port {
			A = 0,
			B = 1
		};

		enum Line {
			One = 0,
			Two = 1
		};

		/*! Sets a register value. */
		inline void set_register(int address, uint8_t value)
		{
			address &= 0xf;
//			printf("6522 %p: %d <- %02x\n", this, address, value);
			switch(address)
			{
				case 0x0:
					_registers.output[1] = value;
					static_cast<T *>(this)->set_port_output(Port::B, value, _registers.data_direction[1]);	// TODO: handshake

					_registers.interrupt_flags &= ~(InterruptFlag::CB1ActiveEdge | InterruptFlag::CB2ActiveEdge);
					reevaluate_interrupts();
				break;
				case 0xf:
				case 0x1:
					_registers.output[0] = value;
					static_cast<T *>(this)->set_port_output(Port::A, value, _registers.data_direction[0]);	// TODO: handshake

					_registers.interrupt_flags &= ~(InterruptFlag::CA1ActiveEdge | InterruptFlag::CA2ActiveEdge);
					reevaluate_interrupts();
				break;
//					// No handshake, so write directly
//					_registers.output[0] = value;
//					static_cast<T *>(this)->set_port_output(0, value);
//				break;

				case 0x2:
					_registers.data_direction[1] = value;
				break;
				case 0x3:
					_registers.data_direction[0] = value;
				break;

				// Timer 1
				case 0x6:	case 0x4:	_registers.timer_latch[0] = (_registers.timer_latch[0]&0xff00) | value;	break;
				case 0x5:	case 0x7:
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
				case 0x8:	_registers.timer_latch[1] = value;	break;
				case 0x9:
					_registers.interrupt_flags &= ~InterruptFlag::Timer2;
					_registers.timer[1] = _registers.timer_latch[1] | (uint16_t)(value << 8);
					_timer_is_running[1] = true;
					reevaluate_interrupts();
				break;

				// Shift
				case 0xa:	_registers.shift = value;				break;

				// Control
				case 0xb: _registers.auxiliary_control = value;		break;
				case 0xc:
					_registers.peripheral_control = value;
					printf("Peripheral control %02x\n", value);
				break;

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
					printf("[%p] Interrupt mask -> %02x\n", this, _registers.interrupt_enable);
				break;
			}
		}

		/*! Gets a register value. */
		inline uint8_t get_register(int address)
		{
			address &= 0xf;
//			printf("6522 %p: %d\n", this, address);
			switch(address)
			{
				case 0x0:
					_registers.interrupt_flags &= ~(InterruptFlag::CB1ActiveEdge | InterruptFlag::CB2ActiveEdge);
					reevaluate_interrupts();
				return get_port_input(Port::B, _registers.data_direction[1], _registers.output[1]);
				case 0xf:	// TODO: handshake, latching
				case 0x1:
					_registers.interrupt_flags &= ~(InterruptFlag::CA1ActiveEdge | InterruptFlag::CA2ActiveEdge);
					reevaluate_interrupts();
				return get_port_input(Port::A, _registers.data_direction[0], _registers.output[0]);

				case 0x2:	return _registers.data_direction[1];
				case 0x3:	return _registers.data_direction[0];

				// Timer 1
				case 0x4:
					_registers.interrupt_flags &= ~InterruptFlag::Timer1;
					reevaluate_interrupts();
				return _registers.timer[0] & 0x00ff;
				case 0x5:	return _registers.timer[0] >> 8;
				case 0x6:	return _registers.timer_latch[0] & 0x00ff;
				case 0x7:	return _registers.timer_latch[0] >> 8;

				// Timer 2
				case 0x8:
					_registers.interrupt_flags &= ~InterruptFlag::Timer2;
					reevaluate_interrupts();
				return _registers.timer[1] & 0x00ff;
				case 0x9:	return _registers.timer[1] >> 8;

				case 0xa:	return _registers.shift;

				case 0xb:	return _registers.auxiliary_control;
				case 0xc:	return _registers.peripheral_control;

				case 0xd:	return _registers.interrupt_flags | (get_interrupt_line() ? 0x80 : 0x00);
				case 0xe:	return _registers.interrupt_enable | 0x80;
			}

			return 0xff;
		}

		inline void set_control_line(Port port, Line line, bool value)
		{
			switch(line)
			{
				case Line::One:
					if(	value != _control_inputs[port].line_one &&
						value == !!(_registers.peripheral_control & (port ? 0x10 : 0x01))
					)
					{
						_registers.interrupt_flags |= port ? InterruptFlag::CB1ActiveEdge : InterruptFlag::CA1ActiveEdge;
						reevaluate_interrupts();
					}
					_control_inputs[port].line_one = value;
				break;

				case Line::Two:
					// TODO
				break;
			}
		}

		/*!
			Runs for a specified number of half cycles.

			Although the original chip accepts only a phase-2 input, timer reloads are specified as occuring
			1.5 cycles after the timer hits zero. It is therefore necessary to emulate at half-cycle precision.

			The first emulated half-cycle will be the period between the trailing edge of a phase-2 input and the
			next rising edge. So it should align with a full system's phase-1. The next emulated half-cycle will be
			that which occurs during phase-2.
		*/
		inline void run_for_half_cycles(unsigned int number_of_cycles)
		{
			while(number_of_cycles--)
			{
				if(_is_phase2)
				{
					_registers.last_timer[0] = _registers.timer[0];
					_registers.last_timer[1] = _registers.timer[1];

					if(_registers.timer_needs_reload)
					{
						_registers.timer_needs_reload = false;
						_registers.timer[0] = _registers.timer_latch[0];
					}
					else
						_registers.timer[0] --;

					_registers.timer[1] --;
				}
				else
				{
					// IRQ is raised on the half cycle after overflow
					if((_registers.timer[1] == 0xffff) && !_registers.last_timer[1] && _timer_is_running[1])
					{
						_timer_is_running[1] = false;
						_registers.interrupt_flags |= InterruptFlag::Timer2;
						reevaluate_interrupts();
					}

					if((_registers.timer[0] == 0xffff) && !_registers.last_timer[0] && _timer_is_running[0])
					{
						_registers.interrupt_flags |= InterruptFlag::Timer1;
						reevaluate_interrupts();

						if(_registers.auxiliary_control&0x40)
							_registers.timer_needs_reload = true;
						else
							_timer_is_running[0] = false;
					}
				}

				_is_phase2 ^= true;
			}
		}

		/*! @returns @c true if the IRQ line is currently active; @c false otherwise. */
		inline bool get_interrupt_line()
		{
			uint8_t interrupt_status = _registers.interrupt_flags & _registers.interrupt_enable & 0x7f;
			return !!interrupt_status;
		}

		MOS6522() :
			_timer_is_running{false, false},
			_last_posted_interrupt_status(false),
			_is_phase2(false)
		{}

	private:
		// Expected to be overridden
		uint8_t get_port_input(Port port)										{	return 0xff;	}
		void set_port_output(Port port, uint8_t value, uint8_t direction_mask)	{}
		bool get_control_line(Port port, Line line)								{	return true;	}
//		void set_interrupt_status(bool status)			{}

		// Input/output multiplexer
		uint8_t get_port_input(Port port, uint8_t output_mask, uint8_t output)
		{
			uint8_t input = static_cast<T *>(this)->get_port_input(port);
			return (input & ~output_mask) | (output & output_mask);
		}

		// Phase toggle
		bool _is_phase2;

		// Delegate and communications
		bool _last_posted_interrupt_status;
		inline void reevaluate_interrupts()
		{
			bool new_interrupt_status = get_interrupt_line();
			if(new_interrupt_status != _last_posted_interrupt_status)
			{
				_last_posted_interrupt_status = new_interrupt_status;
				static_cast<T *>(this)->set_interrupt_status(new_interrupt_status);
			}
		}

		// The registers
		struct Registers {
			uint8_t output[2], input[2], data_direction[2];
			uint16_t timer[2], timer_latch[2], last_timer[2];
			uint8_t shift;
			uint8_t auxiliary_control, peripheral_control;
			uint8_t interrupt_flags, interrupt_enable;
			bool timer_needs_reload;

			// "A  low  reset  (RES)  input  clears  all  R6522  internal registers to logic 0"
			Registers() :
				output{0, 0}, input{0, 0}, data_direction{0, 0},
				auxiliary_control(0), peripheral_control(0),
				interrupt_flags(0), interrupt_enable(0),
				last_timer{0, 0}, timer_needs_reload(false) {}
		} _registers;

		// control state
		struct {
			bool line_one, line_two;
		} _control_inputs[2];

		// Internal state other than the registers
		bool _timer_is_running[2];
};

/*!
	Provided for optional composition with @c MOS6522, @c MOS6522IRQDelegate provides for a delegate
	that will receive IRQ line change notifications.
*/
class MOS6522IRQDelegate {
	public:
		class Delegate {
			public:
				virtual void mos6522_did_change_interrupt_status(void *mos6522) = 0;
		};

		void set_delegate(Delegate *delegate)
		{
			_delegate = delegate;
		}

		void set_interrupt_status(bool new_status)
		{
			if(_delegate) _delegate->mos6522_did_change_interrupt_status(this);
		}

	private:
		Delegate *_delegate;
};

}

#endif /* _522_hpp */
