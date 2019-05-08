//
//  Implementation.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/09/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "../../../Outputs/Log.hpp"

namespace MOS {
namespace MOS6522 {

template <typename T> void MOS6522<T>::set_register(int address, uint8_t value) {
	address &= 0xf;
	switch(address) {
		case 0x0:	// Write Port B.
			registers_.output[1] = value;
			bus_handler_.set_port_output(Port::B, value, registers_.data_direction[1]);

			if(handshake_modes_[0] != HandshakeMode::None) {
				bus_handler_.set_control_line_output(Port::B, Line::Two, false);
			}

			registers_.interrupt_flags &= ~(InterruptFlag::CB1ActiveEdge | ((registers_.peripheral_control&0x20) ? 0 : InterruptFlag::CB2ActiveEdge));
			reevaluate_interrupts();
		break;
		case 0xf:
		case 0x1:	// Write Port A.
			registers_.output[0] = value;
			bus_handler_.set_port_output(Port::A, value, registers_.data_direction[0]);

			if(handshake_modes_[1] != HandshakeMode::None) {
				bus_handler_.set_control_line_output(Port::A, Line::Two, false);
			}

			registers_.interrupt_flags &= ~(InterruptFlag::CA1ActiveEdge | ((registers_.peripheral_control&0x02) ? 0 : InterruptFlag::CB2ActiveEdge));
			reevaluate_interrupts();
		break;

		case 0x2:	// Port B direction.
			registers_.data_direction[1] = value;
		break;
		case 0x3:	// Port A direction.
			registers_.data_direction[0] = value;
		break;

		// Timer 1
		case 0x6:	case 0x4:	registers_.timer_latch[0] = (registers_.timer_latch[0]&0xff00) | value;	break;
		case 0x5:	case 0x7:
			registers_.timer_latch[0] = (registers_.timer_latch[0]&0x00ff) | static_cast<uint16_t>(value << 8);
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
			registers_.next_timer[1] = registers_.timer_latch[1] | static_cast<uint16_t>(value << 8);
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
			registers_.peripheral_control = value;

			// TODO: simplify below; trying to avoid improper logging of unimplemented warnings in input mode
			handshake_modes_[0] = HandshakeMode::None;
			switch(value & 0x0e) {
				default: 	LOG("Unimplemented control line CA2 mode " << int((value >> 1)&7));	break;

				case 0x00:	// Negative interrupt input; set CA2 interrupt on negative CA2 transition, clear on access to Port A register.
				case 0x02:	// Independent negative interrupt input; set CA2 interrupt on negative transition, don't clear automatically.
				case 0x04:	// Positive interrupt input; set CA2 interrupt on positive CA2 transition, clear on access to Port A register.
				case 0x06:	// Independent positive interrupt input; set CA2 interrupt on positive transition, don't clear automatically.
				break;

				case 0x08:	// Handshake: set CA2 to low on any read or write of Port A; set to high on an active transition of CA1.
					handshake_modes_[0] = HandshakeMode::Handshake;
				break;

				case 0x0a:	// Pulse output: CA2 is low for one cycle following a read or write of Port A.
					handshake_modes_[0] = HandshakeMode::Pulse;
				break;

				case 0x0c:	// Manual output: CA2 low.
					bus_handler_.set_control_line_output(Port::A, Line::Two, false);
				break;

				case 0x0e:	// Manual output: CA2 high.
					bus_handler_.set_control_line_output(Port::A, Line::Two, true);
				break;
			}
			switch(value & 0xe0) {
				default: 	LOG("Unimplemented control line CB2 mode " << int((value >> 5)&7));	break;
				case 0xc0:	bus_handler_.set_control_line_output(Port::B, Line::Two, false);	break;
				case 0xe0:	bus_handler_.set_control_line_output(Port::B, Line::Two, true);		break;
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

template <typename T> uint8_t MOS6522<T>::get_register(int address) {
	address &= 0xf;
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

template <typename T> uint8_t MOS6522<T>::get_port_input(Port port, uint8_t output_mask, uint8_t output) {
	uint8_t input = bus_handler_.get_port_input(port);
	return (input & ~output_mask) | (output & output_mask);
}

template <typename T> T &MOS6522<T>::bus_handler() {
	return bus_handler_;
}

// Delegate and communications
template <typename T> void MOS6522<T>::reevaluate_interrupts() {
	bool new_interrupt_status = get_interrupt_line();
	if(new_interrupt_status != last_posted_interrupt_status_) {
		last_posted_interrupt_status_ = new_interrupt_status;
		bus_handler_.set_interrupt_status(new_interrupt_status);
	}
}

template <typename T> void MOS6522<T>::set_control_line_input(Port port, Line line, bool value) {
	switch(line) {
		case Line::One:
			if(	value != control_inputs_[port].line_one &&
				value == !!(registers_.peripheral_control & (port ? 0x10 : 0x01))
			) {
				if(handshake_modes_[port] == HandshakeMode::Handshake) {
//					bus_handler_
				}

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

template <typename T> void MOS6522<T>::do_phase2() {
	registers_.last_timer[0] = registers_.timer[0];
	registers_.last_timer[1] = registers_.timer[1];

	if(registers_.timer_needs_reload) {
		registers_.timer_needs_reload = false;
		registers_.timer[0] = registers_.timer_latch[0];
	} else {
		registers_.timer[0] --;
	}

	registers_.timer[1] --;
	if(registers_.next_timer[0] >= 0) {
		registers_.timer[0] = static_cast<uint16_t>(registers_.next_timer[0]);
		registers_.next_timer[0] = -1;
	}
	if(registers_.next_timer[1] >= 0) {
		registers_.timer[1] = static_cast<uint16_t>(registers_.next_timer[1]);
		registers_.next_timer[1] = -1;
	}
}

template <typename T> void MOS6522<T>::do_phase1() {
	// IRQ is raised on the half cycle after overflow
	if((registers_.timer[1] == 0xffff) && !registers_.last_timer[1] && timer_is_running_[1]) {
		timer_is_running_[1] = false;
		registers_.interrupt_flags |= InterruptFlag::Timer2;
		reevaluate_interrupts();
	}

	if((registers_.timer[0] == 0xffff) && !registers_.last_timer[0] && timer_is_running_[0]) {
		registers_.interrupt_flags |= InterruptFlag::Timer1;
		reevaluate_interrupts();

		if(registers_.auxiliary_control&0x40)
			registers_.timer_needs_reload = true;
		else
			timer_is_running_[0] = false;
	}
}

/*! Runs for a specified number of half cycles. */
template <typename T> void MOS6522<T>::run_for(const HalfCycles half_cycles) {
	int number_of_half_cycles = half_cycles.as_int();

	if(is_phase2_) {
		do_phase2();
		number_of_half_cycles--;
	}

	while(number_of_half_cycles >= 2) {
		do_phase1();
		do_phase2();
		number_of_half_cycles -= 2;
	}

	if(number_of_half_cycles) {
		do_phase1();
		is_phase2_ = true;
	} else {
		is_phase2_ = false;
	}
}

/*! Runs for a specified number of cycles. */
template <typename T> void MOS6522<T>::run_for(const Cycles cycles) {
	int number_of_cycles = cycles.as_int();
	while(number_of_cycles--) {
		do_phase1();
		do_phase2();
	}
}

/*! @returns @c true if the IRQ line is currently active; @c false otherwise. */
template <typename T> bool MOS6522<T>::get_interrupt_line() {
	uint8_t interrupt_status = registers_.interrupt_flags & registers_.interrupt_enable & 0x7f;
	return !!interrupt_status;
}

}
}
