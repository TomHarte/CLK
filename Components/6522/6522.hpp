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
#include <typeinfo>
#include <cstdio>

#include "../../ClockReceiver/ClockReceiver.hpp"

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
template <class T> class MOS6522: public ClockReceiver<MOS6522<T>> {
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
		inline void set_register(int address, uint8_t value) {
			address &= 0xf;
//			printf("6522 [%s]: %0x <- %02x\n", typeid(*this).name(), address, value);
			switch(address) {
				case 0x0:
					registers_.output[1] = value;
					static_cast<T *>(this)->set_port_output(Port::B, value, registers_.data_direction[1]);	// TODO: handshake

					registers_.interrupt_flags &= ~(InterruptFlag::CB1ActiveEdge | ((registers_.peripheral_control&0x20) ? 0 : InterruptFlag::CB2ActiveEdge));
					reevaluate_interrupts();
				break;
				case 0xf:
				case 0x1:
					registers_.output[0] = value;
					static_cast<T *>(this)->set_port_output(Port::A, value, registers_.data_direction[0]);	// TODO: handshake

					registers_.interrupt_flags &= ~(InterruptFlag::CA1ActiveEdge | ((registers_.peripheral_control&0x02) ? 0 : InterruptFlag::CB2ActiveEdge));
					reevaluate_interrupts();
				break;
//					// No handshake, so write directly
//					registers_.output[0] = value;
//					static_cast<T *>(this)->set_port_output(0, value);
//				break;

				case 0x2:
					registers_.data_direction[1] = value;
				break;
				case 0x3:
					registers_.data_direction[0] = value;
				break;

				// Timer 1
				case 0x6:	case 0x4:	registers_.timer_latch[0] = (registers_.timer_latch[0]&0xff00) | value;	break;
				case 0x5:	case 0x7:
					registers_.timer_latch[0] = (registers_.timer_latch[0]&0x00ff) | (uint16_t)(value << 8);
					registers_.interrupt_flags &= ~InterruptFlag::Timer1;
					if(address == 0x05) {
						registers_.next_timer[0] = registers_.timer_latch[0];
						timer_is_running_[0] = true;
					}
					reevaluate_interrupts();
				break;

				// Timer 2
				case 0x8:	registers_.timer_latch[1] = value;	break;
				case 0x9:
					registers_.interrupt_flags &= ~InterruptFlag::Timer2;
					registers_.next_timer[1] = registers_.timer_latch[1] | (uint16_t)(value << 8);
					timer_is_running_[1] = true;
					reevaluate_interrupts();
				break;

				// Shift
				case 0xa:	registers_.shift = value;				break;

				// Control
				case 0xb:
					registers_.auxiliary_control = value;
				break;
				case 0xc:
//					printf("Peripheral control %02x\n", value);
					registers_.peripheral_control = value;

					// TODO: simplify below; trying to avoid improper logging of unimplemented warnings in input mode
					if(value & 0x08) {
						switch(value & 0x0e) {
							default: printf("Unimplemented control line mode %d\n", (value >> 1)&7); break;
							case 0x0c:	static_cast<T *>(this)->set_control_line_output(Port::A, Line::Two, false);		break;
							case 0x0e:	static_cast<T *>(this)->set_control_line_output(Port::A, Line::Two, true);		break;
						}
					}
					if(value & 0x80) {
						switch(value & 0xe0) {
							default: printf("Unimplemented control line mode %d\n", (value >> 5)&7); break;
							case 0xc0:	static_cast<T *>(this)->set_control_line_output(Port::B, Line::Two, false);		break;
							case 0xe0:	static_cast<T *>(this)->set_control_line_output(Port::B, Line::Two, true);		break;
						}
					}
				break;

				// Interrupt control
				case 0xd:
					registers_.interrupt_flags &= ~value;
					reevaluate_interrupts();
				break;
				case 0xe:
					if(value&0x80)
						registers_.interrupt_enable |= value;
					else
						registers_.interrupt_enable &= ~value;
					reevaluate_interrupts();
				break;
			}
		}

		/*! Gets a register value. */
		inline uint8_t get_register(int address) {
			address &= 0xf;
//			printf("6522 %p: %d\n", this, address);
			switch(address) {
				case 0x0:
					registers_.interrupt_flags &= ~(InterruptFlag::CB1ActiveEdge | InterruptFlag::CB2ActiveEdge);
					reevaluate_interrupts();
				return get_port_input(Port::B, registers_.data_direction[1], registers_.output[1]);
				case 0xf:	// TODO: handshake, latching
				case 0x1:
					registers_.interrupt_flags &= ~(InterruptFlag::CA1ActiveEdge | InterruptFlag::CA2ActiveEdge);
					reevaluate_interrupts();
				return get_port_input(Port::A, registers_.data_direction[0], registers_.output[0]);

				case 0x2:	return registers_.data_direction[1];
				case 0x3:	return registers_.data_direction[0];

				// Timer 1
				case 0x4:
					registers_.interrupt_flags &= ~InterruptFlag::Timer1;
					reevaluate_interrupts();
				return registers_.timer[0] & 0x00ff;
				case 0x5:	return registers_.timer[0] >> 8;
				case 0x6:	return registers_.timer_latch[0] & 0x00ff;
				case 0x7:	return registers_.timer_latch[0] >> 8;

				// Timer 2
				case 0x8:
					registers_.interrupt_flags &= ~InterruptFlag::Timer2;
					reevaluate_interrupts();
				return registers_.timer[1] & 0x00ff;
				case 0x9:	return registers_.timer[1] >> 8;

				case 0xa:	return registers_.shift;

				case 0xb:	return registers_.auxiliary_control;
				case 0xc:	return registers_.peripheral_control;

				case 0xd:	return registers_.interrupt_flags | (get_interrupt_line() ? 0x80 : 0x00);
				case 0xe:	return registers_.interrupt_enable | 0x80;
			}

			return 0xff;
		}

		inline void set_control_line_input(Port port, Line line, bool value) {
			switch(line) {
				case Line::One:
					if(	value != control_inputs_[port].line_one &&
						value == !!(registers_.peripheral_control & (port ? 0x10 : 0x01))
					) {
						registers_.interrupt_flags |= port ? InterruptFlag::CB1ActiveEdge : InterruptFlag::CA1ActiveEdge;
						reevaluate_interrupts();
					}
					control_inputs_[port].line_one = value;
				break;

				case Line::Two:
					// TODO: output modes, but probably elsewhere?
					if(	value != control_inputs_[port].line_two &&							// i.e. value has changed ...
						!(registers_.peripheral_control & (port ? 0x80 : 0x08)) &&			// ... and line is input ...
						value == !!(registers_.peripheral_control & (port ? 0x40 : 0x04))	// ... and it's either high or low, as required
					) {
						registers_.interrupt_flags |= port ? InterruptFlag::CB2ActiveEdge : InterruptFlag::CA2ActiveEdge;
						reevaluate_interrupts();
					}
					control_inputs_[port].line_two = value;
				break;
			}
		}

#define phase2()	\
	registers_.last_timer[0] = registers_.timer[0];\
	registers_.last_timer[1] = registers_.timer[1];\
\
	if(registers_.timer_needs_reload) {\
		registers_.timer_needs_reload = false;\
		registers_.timer[0] = registers_.timer_latch[0];\
	}\
	else\
		registers_.timer[0] --;\
\
	registers_.timer[1] --; \
	if(registers_.next_timer[0] >= 0) { registers_.timer[0] = (uint16_t)registers_.next_timer[0]; registers_.next_timer[0] = -1; }\
	if(registers_.next_timer[1] >= 0) { registers_.timer[1] = (uint16_t)registers_.next_timer[1]; registers_.next_timer[1] = -1; }\

	// IRQ is raised on the half cycle after overflow
#define phase1()	\
	if((registers_.timer[1] == 0xffff) && !registers_.last_timer[1] && timer_is_running_[1]) {\
		timer_is_running_[1] = false;\
		registers_.interrupt_flags |= InterruptFlag::Timer2;\
		reevaluate_interrupts();\
	}\
\
	if((registers_.timer[0] == 0xffff) && !registers_.last_timer[0] && timer_is_running_[0]) {\
		registers_.interrupt_flags |= InterruptFlag::Timer1;\
		reevaluate_interrupts();\
\
		if(registers_.auxiliary_control&0x40)\
			registers_.timer_needs_reload = true;\
		else\
			timer_is_running_[0] = false;\
	}

		/*! Runs for a specified number of half cycles. */
		inline void run_for(const HalfCycles &half_cycles) {
			int number_of_half_cycles = half_cycles.as_int();

			if(is_phase2_) {
				phase2();
				number_of_half_cycles--;
			}

			while(number_of_half_cycles >= 2) {
				phase1();
				phase2();
				number_of_half_cycles -= 2;
			}

			if(number_of_half_cycles) {
				phase1();
				is_phase2_ = true;
			} else {
				is_phase2_ = false;
			}
		}

		/*! Runs for a specified number of cycles. */
		inline void run_for(const Cycles &cycles) {
			int number_of_cycles = cycles.as_int();
			while(number_of_cycles--) {
				phase1();
				phase2();
			}
		}

#undef phase1
#undef phase2

		/*! @returns @c true if the IRQ line is currently active; @c false otherwise. */
		inline bool get_interrupt_line() {
			uint8_t interrupt_status = registers_.interrupt_flags & registers_.interrupt_enable & 0x7f;
			return !!interrupt_status;
		}

		MOS6522() :
			timer_is_running_{false, false},
			last_posted_interrupt_status_(false),
			is_phase2_(false) {}

	private:
		// Expected to be overridden
		uint8_t get_port_input(Port port)										{	return 0xff;	}
		void set_port_output(Port port, uint8_t value, uint8_t direction_mask)	{}
		void set_control_line_output(Port port, Line line, bool value)			{}
		void set_interrupt_status(bool status)									{}

		// Input/output multiplexer
		uint8_t get_port_input(Port port, uint8_t output_mask, uint8_t output) {
			uint8_t input = static_cast<T *>(this)->get_port_input(port);
			return (input & ~output_mask) | (output & output_mask);
		}

		// Phase toggle
		bool is_phase2_;

		// Delegate and communications
		bool last_posted_interrupt_status_;
		inline void reevaluate_interrupts() {
			bool new_interrupt_status = get_interrupt_line();
			if(new_interrupt_status != last_posted_interrupt_status_) {
				last_posted_interrupt_status_ = new_interrupt_status;
				static_cast<T *>(this)->set_interrupt_status(new_interrupt_status);
			}
		}

		// The registers
		struct Registers {
			uint8_t output[2], input[2], data_direction[2];
			uint16_t timer[2], timer_latch[2], last_timer[2];
			int next_timer[2];
			uint8_t shift;
			uint8_t auxiliary_control, peripheral_control;
			uint8_t interrupt_flags, interrupt_enable;
			bool timer_needs_reload;

			// "A  low  reset  (RES)  input  clears  all  R6522  internal registers to logic 0"
			Registers() :
				output{0, 0}, input{0, 0}, data_direction{0, 0},
				auxiliary_control(0), peripheral_control(0),
				interrupt_flags(0), interrupt_enable(0),
				last_timer{0, 0}, timer_needs_reload(false),
				next_timer{-1, -1} {}
		} registers_;

		// control state
		struct {
			bool line_one, line_two;
		} control_inputs_[2];

		// Internal state other than the registers
		bool timer_is_running_[2];
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

		inline void set_interrupt_delegate(Delegate *delegate) {
			delegate_ = delegate;
		}

		inline void set_interrupt_status(bool new_status) {
			if(delegate_) delegate_->mos6522_did_change_interrupt_status(this);
		}

	private:
		Delegate *delegate_;
};

}

#endif /* _522_hpp */
