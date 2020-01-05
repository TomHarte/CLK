//
//  6522.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/06/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
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

/*!
	Provides the mechanism for just-int-time communication from a 6522; the normal use case is to compose a
	6522 and a subclass of PortHandler in order to reproduce a 6522 and its original bus wiring.
*/
class PortHandler {
	public:
		/// Requests the current input value of @c port from the port handler.
		uint8_t get_port_input(Port port)										{	return 0xff;	}

		/// Sets the current output value of @c port and provides @c direction_mask, indicating which pins are marked as output.
		void set_port_output(Port port, uint8_t value, uint8_t direction_mask)	{}

		/// Sets the current logical output level for line @c line on port @c port.
		void set_control_line_output(Port port, Line line, bool value)			{}

		/// Sets the current logical value of the interrupt line.
		void set_interrupt_status(bool status)									{}

		/// Provides a measure of time elapsed between other calls.
		void run_for(HalfCycles duration)										{}

		/// Receives passed-on flush() calls from the 6522.
		void flush()															{}
};

/*!
	Provided as an optional alternative base to @c PortHandler for port handlers; via the delegate pattern adds
	a virtual level of indirection for receiving changes to the interrupt line.
*/
class IRQDelegatePortHandler: public PortHandler {
	public:
		class Delegate {
			public:
				/// Indicates that the interrupt status has changed for the IRQDelegatePortHandler provided.
				virtual void mos6522_did_change_interrupt_status(void *irq_delegate) = 0;
		};

		/// Sets the delegate that will receive notification of changes in the interrupt line.
		void set_interrupt_delegate(Delegate *delegate);

		/// Overrides PortHandler::set_interrupt_status, notifying the delegate if one is set.
		void set_interrupt_status(bool new_status);

	private:
		Delegate *delegate_ = nullptr;
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
template <class T> class MOS6522: public MOS6522Storage {
	public:
		MOS6522(T &bus_handler) noexcept : bus_handler_(bus_handler) {}
		MOS6522(const MOS6522 &) = delete;

		/*! Sets a register value. */
		void write(int address, uint8_t value);

		/*! Gets a register value. */
		uint8_t read(int address);

		/*! @returns the bus handler. */
		T &bus_handler();

		/// Sets the input value of line @c line on port @c port.
		void set_control_line_input(Port port, Line line, bool value);

		/// Runs for a specified number of half cycles.
		void run_for(const HalfCycles half_cycles);

		/// Runs for a specified number of cycles.
		void run_for(const Cycles cycles);

		/// @returns @c true if the IRQ line is currently active; @c false otherwise.
		bool get_interrupt_line();

		/// Updates the port handler to the current time and then requests that it flush.
		void flush();

	private:
		void do_phase1();
		void do_phase2();
		void shift_in();
		void shift_out();

		T &bus_handler_;
		HalfCycles time_since_bus_handler_call_;

		void access(int address);

		uint8_t get_port_input(Port port, uint8_t output_mask, uint8_t output);
		inline void reevaluate_interrupts();

		/// Sets the current intended output value for the port and line;
		/// if this affects the visible output, it will be passed to the handler.
		void set_control_line_output(Port port, Line line, LineState value);
		void evaluate_cb2_output();
};

}
}

#include "Implementation/6522Implementation.hpp"

#endif /* _522_hpp */
