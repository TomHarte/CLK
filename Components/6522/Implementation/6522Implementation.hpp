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

template <typename T> void MOS6522<T>::access(int address) {
	switch(address) {
		case 0x0:
			// In both handshake and pulse modes, CB2 goes low on any read or write of Port B.
			if(handshake_modes_[1] != HandshakeMode::None) {
				set_control_line_output(Port::B, Line::Two, false);
			}
		break;

		case 0xf:
		case 0x1:
			// In both handshake and pulse modes, CA2 goes low on any read or write of Port A.
			if(handshake_modes_[0] != HandshakeMode::None) {
				set_control_line_output(Port::A, Line::Two, false);
			}
		break;
	}
}

template <typename T> void MOS6522<T>::set_register(int address, uint8_t value) {
	address &= 0xf;
	access(address);
	switch(address) {
		case 0x0:	// Write Port B.
			// Store locally and communicate outwards.
			registers_.output[1] = value;

			bus_handler_.run_for(time_since_bus_handler_call_.flush());
			bus_handler_.set_port_output(Port::B, value, registers_.data_direction[1]);

			registers_.interrupt_flags &= ~(InterruptFlag::CB1ActiveEdge | ((registers_.peripheral_control&0x20) ? 0 : InterruptFlag::CB2ActiveEdge));
			reevaluate_interrupts();
		break;
		case 0xf:
		case 0x1:	// Write Port A.
			registers_.output[0] = value;

			bus_handler_.run_for(time_since_bus_handler_call_.flush());
			bus_handler_.set_port_output(Port::A, value, registers_.data_direction[0]);

			if(handshake_modes_[1] != HandshakeMode::None) {
				set_control_line_output(Port::A, Line::Two, false);
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
		case 0xa:
			registers_.shift = value;
			shift_bits_remaining_ = 8;
		break;

		// Control
		case 0xb:
			registers_.auxiliary_control = value;
		break;
		case 0xc: {
//			const auto old_peripheral_control = registers_.peripheral_control;
			registers_.peripheral_control = value;

			int shift = 0;
			for(int port = 0; port < 2; ++port) {
				handshake_modes_[port] = HandshakeMode::None;
				switch((value >> shift) & 0x0e) {
					default: break;

					case 0x00:	// Negative interrupt input; set CA2 interrupt on negative CA2 transition, clear on access to Port A register.
					case 0x02:	// Independent negative interrupt input; set CA2 interrupt on negative transition, don't clear automatically.
					case 0x04:	// Positive interrupt input; set CA2 interrupt on positive CA2 transition, clear on access to Port A register.
					case 0x06:	// Independent positive interrupt input; set CA2 interrupt on positive transition, don't clear automatically.
					break;

					case 0x08:	// Handshake: set CA2 to low on any read or write of Port A; set to high on an active transition of CA1.
						handshake_modes_[port] = HandshakeMode::Handshake;
					break;

					case 0x0a:	// Pulse output: CA2 is low for one cycle following a read or write of Port A.
						handshake_modes_[port] = HandshakeMode::Pulse;
					break;

					case 0x0c:	// Manual output: CA2 low.
						set_control_line_output(Port(port), Line::Two, false);
					break;

					case 0x0e:	// Manual output: CA2 high.
						set_control_line_output(Port(port), Line::Two, true);
					break;
				}

				shift += 4;
			}
		} break;

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
	access(address);
	switch(address) {
		case 0x0:
			registers_.interrupt_flags &= ~(InterruptFlag::CB1ActiveEdge | InterruptFlag::CB2ActiveEdge);
			reevaluate_interrupts();
		return get_port_input(Port::B, registers_.data_direction[1], registers_.output[1]);
		case 0xf:
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

		case 0xa:
			shift_bits_remaining_ = 8;
		return registers_.shift;

		case 0xb:	return registers_.auxiliary_control;
		case 0xc:	return registers_.peripheral_control;

		case 0xd:	return registers_.interrupt_flags | (get_interrupt_line() ? 0x80 : 0x00);
		case 0xe:	return registers_.interrupt_enable | 0x80;
	}

	return 0xff;
}

template <typename T> uint8_t MOS6522<T>::get_port_input(Port port, uint8_t output_mask, uint8_t output) {
	bus_handler_.run_for(time_since_bus_handler_call_.flush());
	const uint8_t input = bus_handler_.get_port_input(port);
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

		bus_handler_.run_for(time_since_bus_handler_call_.flush());
		bus_handler_.set_interrupt_status(new_interrupt_status);
	}
}

template <typename T> void MOS6522<T>::set_control_line_input(Port port, Line line, bool value) {
	switch(line) {
		case Line::One:
			if(	value != control_inputs_[port].lines[line]) {
				// In handshake mode, any transition on C[A/B]1 sets output high on C[A/B]2.
				if(handshake_modes_[port] == HandshakeMode::Handshake) {
					set_control_line_output(port, Line::Two, true);
				}

				// Set the proper transition interrupt bit if enabled.
				if(value == !!(registers_.peripheral_control & (port ? 0x10 : 0x01))) {
					registers_.interrupt_flags |= port ? InterruptFlag::CB1ActiveEdge : InterruptFlag::CA1ActiveEdge;
					reevaluate_interrupts();
				}

				// If this is a low-to-high transition, consider updating the shift register.
				if(value) {
					switch((registers_.auxiliary_control >> 2)&7) {
						default: 					break;
						case 3:		shift_in();		break;
						case 7:		shift_out();	break;
					}
				}
			}
			control_inputs_[port].lines[line] = value;
		break;

		case Line::Two:
			if(	value != control_inputs_[port].lines[line] &&						// i.e. value has changed ...
				!(registers_.peripheral_control & (port ? 0x80 : 0x08)) &&			// ... and line is input ...
				value == !!(registers_.peripheral_control & (port ? 0x40 : 0x04))	// ... and it's either high or low, as required
			) {
				registers_.interrupt_flags |= port ? InterruptFlag::CB2ActiveEdge : InterruptFlag::CA2ActiveEdge;
				reevaluate_interrupts();
			}
			control_inputs_[port].lines[line] = value;
		break;
	}
}

template <typename T> void MOS6522<T>::do_phase2() {
	++ time_since_bus_handler_call_;

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

	// In pulse modes, CA2 and CB2 go high again on the next clock edge.
	if(handshake_modes_[1] == HandshakeMode::Pulse) {
		set_control_line_output(Port::B, Line::Two, true);
	}
	if(handshake_modes_[0] == HandshakeMode::Pulse) {
		set_control_line_output(Port::A, Line::Two, true);
	}
}

template <typename T> void MOS6522<T>::do_phase1() {
	++ time_since_bus_handler_call_;

	// IRQ is raised on the half cycle after overflow
	if((registers_.timer[1] == 0xffff) && !registers_.last_timer[1] && timer_is_running_[1]) {
		timer_is_running_[1] = false;

		// If the shift register is shifting according to this timer, do a shift.
		// TODO: "shift register is driven by only the low order 8 bits of timer 2"?
		switch((registers_.auxiliary_control >> 2)&7) {
			default: 							break;
			case 1:				shift_in();		break;
			case 4: 			shift_out();	break;
			case 5:				shift_out();	break;	// TODO: present a clock on CB1.
		}

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

	// If the shift register is shifting according to the input clock, do a shift.
	switch((registers_.auxiliary_control >> 2)&7) {
		default: 					break;
		case 2:		shift_in();		break;
		case 6:		shift_out();	break;
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

template <typename T> void MOS6522<T>::flush() {
	bus_handler_.run_for(time_since_bus_handler_call_.flush());
	bus_handler_.flush();
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

template <typename T> void MOS6522<T>::set_control_line_output(Port port, Line line, bool value, bool was_output) {
	// Don't announce an unchanged value.
	if(control_outputs_[port].lines[line] == value && was_output)
		return;

	// Store the value as the intended output, announce it only if this
	// control line is actually in output mode.
	control_outputs_[port].lines[line] = value;
	if(registers_.peripheral_control & (0x08 << (port * 4))) {
		bus_handler_.run_for(time_since_bus_handler_call_.flush());
		bus_handler_.set_control_line_output(port, line, value);
	}
}

template <typename T> void MOS6522<T>::shift_in() {
	registers_.shift = uint8_t((registers_.shift << 1) | (control_inputs_[1].lines[1] ? 1 : 0));
	--shift_bits_remaining_;
	if(!shift_bits_remaining_) {
		registers_.interrupt_flags |= InterruptFlag::ShiftRegister;
		reevaluate_interrupts();
	}
}

template <typename T> void MOS6522<T>::shift_out() {
	set_control_line_output(Port::B, Line::Two, registers_.shift & 0x80);
	registers_.shift <<= 1;
	--shift_bits_remaining_;
	if(!shift_bits_remaining_) {
		registers_.interrupt_flags |= InterruptFlag::ShiftRegister;
		reevaluate_interrupts();
	}
}

}
}
