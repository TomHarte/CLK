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

#include "Implementation/6522Storage.hpp"

#include "../../ClockReceiver/ClockReceiver.hpp"

namespace MOS {
namespace MOS6522 {

enum Port {
	A = 0,
	B = 1
};

enum Line {
	One = 0,
	Two = 1
};

class PortHandler {
	public:
		uint8_t get_port_input(Port port)										{	return 0xff;	}
		void set_port_output(Port port, uint8_t value, uint8_t direction_mask)	{}
		void set_control_line_output(Port port, Line line, bool value)			{}
		void set_interrupt_status(bool status)									{}
};

/*!
	Provided for optional composition with @c MOS6522, @c MOS6522IRQDelegate provides for a delegate
	that will receive IRQ line change notifications.
*/
class IRQDelegatePortHandler: public PortHandler {
	public:
		class Delegate {
			public:
				virtual void mos6522_did_change_interrupt_status(void *mos6522) = 0;
		};

		void set_interrupt_delegate(Delegate *delegate);
		void set_interrupt_status(bool new_status);

	private:
		Delegate *delegate_ = nullptr;
};

class MOS6522Base: public MOS6522Storage {
	public:
		void set_control_line_input(Port port, Line line, bool value);

		/*! Runs for a specified number of half cycles. */
		void run_for(const HalfCycles half_cycles);

		/*! Runs for a specified number of cycles. */
		void run_for(const Cycles cycles);

		/*! @returns @c true if the IRQ line is currently active; @c false otherwise. */
		bool get_interrupt_line();

	private:
		inline void do_phase1();
		inline void do_phase2();
		virtual void reevaluate_interrupts() = 0;
};

/*!
	Implements a template for emulation of the MOS 6522 Versatile Interface Adaptor ('VIA').

	The VIA provides:
		* two timers, each of which may trigger interrupts and one of which may repeat;
		* two digial input/output ports; and
		* a serial-to-parallel shifter.

	Consumers should derive their own curiously-recurring-template-pattern subclass,
	implementing bus communications as required.
*/
template <class T> class MOS6522: public MOS6522Base {
	public:
		MOS6522(T &bus_handler) : bus_handler_(bus_handler) {}

		/*! Sets a register value. */
		void set_register(int address, uint8_t value);

		/*! Gets a register value. */
		uint8_t get_register(int address);

	private:
		T &bus_handler_;

		uint8_t get_port_input(Port port, uint8_t output_mask, uint8_t output);
		inline void reevaluate_interrupts();
};

#include "Implementation/6522Implementation.hpp"

}
}

#endif /* _522_hpp */
