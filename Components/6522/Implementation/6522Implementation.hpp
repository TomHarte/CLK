//
//  Implementation.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/09/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "../../../Outputs/Log.hpp"

// As-yet unimplemented (incomplete list):
//
//	PB6 count-down mode for timer 2.

namespace MOS {
namespace MOS6522 {

template <typename T> void MOS6522<T>::access(int address) {
	switch(address) {
		case 0x0:
			// In both handshake and pulse modes, CB2 goes low on any read or write of Port B.
			if(handshake_modes_[1] != HandshakeMode::None) {
				set_control_line_output(Port::B, Line::Two, LineState::Off);
			}
		break;

		case 0xf:
		case 0x1:
			// In both handshake and pulse modes, CA2 goes low on any read or write of Port A.
			if(handshake_modes_[0] != HandshakeMode::None) {
				set_control_line_output(Port::A, Line::Two, LineState::Off);
			}
		break;
	}
}

template <typename T> void MOS6522<T>::write(int address, uint8_t value) {
	address &= 0xf;
	access(address);
	switch(address) {
		case 0x0:	// Write Port B. ('ORB')
			// Store locally and communicate outwards.
			registers_.output[1] = value;

			bus_handler_.run_for(time_since_bus_handler_call_.flush<HalfCycles>());
			evaluate_port_b_output();

			registers_.interrupt_flags &= ~(InterruptFlag::CB1ActiveEdge | ((registers_.peripheral_control&0x20) ? 0 : InterruptFlag::CB2ActiveEdge));
			reevaluate_interrupts();
		break;
		case 0xf:
		case 0x1:	// Write Port A. ('ORA')
			registers_.output[0] = value;

			bus_handler_.run_for(time_since_bus_handler_call_.flush<HalfCycles>());
			bus_handler_.set_port_output(Port::A, value, registers_.data_direction[0]);

			if(handshake_modes_[1] != HandshakeMode::None) {
				set_control_line_output(Port::A, Line::Two, LineState::Off);
			}

			registers_.interrupt_flags &= ~(InterruptFlag::CA1ActiveEdge | ((registers_.peripheral_control&0x02) ? 0 : InterruptFlag::CB2ActiveEdge));
			reevaluate_interrupts();
		break;

		case 0x2:	// Port B direction ('DDRB').
			registers_.data_direction[1] = value;
		break;
		case 0x3:	// Port A direction ('DDRA').
			registers_.data_direction[0] = value;
		break;

		// Timer 1
		case 0x6:	case 0x4:	// ('T1L-L' and 'T1C-L')
			registers_.timer_latch[0] = (registers_.timer_latch[0]&0xff00) | value;
		break;
		case 0x7:	// Timer 1 latch, high ('T1L-H').
			registers_.timer_latch[0] = (registers_.timer_latch[0]&0x00ff) | uint16_t(value << 8);
		break;
		case 0x5:	// Timer 1 counter, high ('T1C-H').
			// Fill latch.
			registers_.timer_latch[0] = (registers_.timer_latch[0]&0x00ff) | uint16_t(value << 8);

			// Restart timer.
			registers_.next_timer[0] = registers_.timer_latch[0];
			timer_is_running_[0] = true;

			// If PB7 output mode is active, set it low.
			if(registers_.auxiliary_control & 0x80) {
				registers_.timer_port_b_output &= 0x7f;
				evaluate_port_b_output();
			}

			// Clear existing interrupt flag.
			registers_.interrupt_flags &= ~InterruptFlag::Timer1;
			reevaluate_interrupts();
		break;

		// Timer 2
		case 0x8:	// ('T2C-L')
			registers_.timer_latch[1] = value;
		break;
		case 0x9:	// ('T2C-H')
			registers_.interrupt_flags &= ~InterruptFlag::Timer2;
			registers_.next_timer[1] = registers_.timer_latch[1] | uint16_t(value << 8);
			timer_is_running_[1] = true;
			reevaluate_interrupts();
		break;

		// Shift
		case 0xa:	// ('SR')
			registers_.shift = value;
			shift_bits_remaining_ = 8;
			registers_.interrupt_flags &= ~InterruptFlag::ShiftRegister;
			reevaluate_interrupts();
		break;

		// Control
		case 0xb:	// Auxiliary control ('ACR').
			registers_.auxiliary_control = value;
			evaluate_cb2_output();

			// This is a bit of a guess: reset the timer-based PB7 output to its default high level
			// any timer that timer-linked PB7 output is disabled.
			if(!(registers_.auxiliary_control & 0x80)) {
				registers_.timer_port_b_output |= 0x80;
			}
			evaluate_port_b_output();
		break;
		case 0xc: {	// Peripheral control ('PCR').
//			const auto old_peripheral_control = registers_.peripheral_control;
			registers_.peripheral_control = value;

			int shift = 0;
			for(int port = 0; port < 2; ++port) {
				handshake_modes_[port] = HandshakeMode::None;
				switch((value >> shift) & 0x0e) {
					default: break;

					case 0x00:	// Negative interrupt input; set Cx2 interrupt on negative Cx2 transition, clear on access to Port x register.
					case 0x02:	// Independent negative interrupt input; set Cx2 interrupt on negative transition, don't clear automatically.
					case 0x04:	// Positive interrupt input; set Cx2 interrupt on positive Cx2 transition, clear on access to Port x register.
					case 0x06:	// Independent positive interrupt input; set Cx2 interrupt on positive transition, don't clear automatically.
						set_control_line_output(Port(port), Line::Two, LineState::Input);
					break;

					case 0x08:	// Handshake: set Cx2 to low on any read or write of Port x; set to high on an active transition of Cx1.
						handshake_modes_[port] = HandshakeMode::Handshake;
						set_control_line_output(Port(port), Line::Two, LineState::Off);	// At a guess.
					break;

					case 0x0a:	// Pulse output: Cx2 is low for one cycle following a read or write of Port x.
						handshake_modes_[port] = HandshakeMode::Pulse;
						set_control_line_output(Port(port), Line::Two, LineState::On);
					break;

					case 0x0c:	// Manual output: Cx2 low.
						set_control_line_output(Port(port), Line::Two, LineState::Off);
					break;

					case 0x0e:	// Manual output: Cx2 high.
						set_control_line_output(Port(port), Line::Two, LineState::On);
					break;
				}

				shift += 4;
			}
		} break;

		// Interrupt control
		case 0xd:	// Interrupt flag regiser ('IFR').
			registers_.interrupt_flags &= ~value;
			reevaluate_interrupts();
		break;
		case 0xe:	// Interrupt enable register ('IER').
			if(value&0x80)
				registers_.interrupt_enable |= value;
			else
				registers_.interrupt_enable &= ~value;
			reevaluate_interrupts();
		break;
	}
}

template <typename T> uint8_t MOS6522<T>::read(int address) {
	address &= 0xf;
	access(address);
	switch(address) {
		case 0x0:	// Read Port B ('IRB').
			registers_.interrupt_flags &= ~(InterruptFlag::CB1ActiveEdge | InterruptFlag::CB2ActiveEdge);
			reevaluate_interrupts();
		return get_port_input(Port::B, registers_.data_direction[1], registers_.output[1], registers_.auxiliary_control & 0x80);
		case 0xf:
		case 0x1:	// Read Port A ('IRA').
			registers_.interrupt_flags &= ~(InterruptFlag::CA1ActiveEdge | InterruptFlag::CA2ActiveEdge);
			reevaluate_interrupts();
		return get_port_input(Port::A, registers_.data_direction[0], registers_.output[0], 0);

		case 0x2:	return registers_.data_direction[1];	// Port B direction ('DDRB').
		case 0x3:	return registers_.data_direction[0];	// Port A direction ('DDRA').

		// Timer 1
		case 0x4:	// Timer 1 low-order latches ('T1L-L').
			registers_.interrupt_flags &= ~InterruptFlag::Timer1;
			reevaluate_interrupts();
		return registers_.timer[0] & 0x00ff;
		case 0x5:	return registers_.timer[0] >> 8;			// Timer 1 high-order counter ('T1C-H')
		case 0x6:	return registers_.timer_latch[0] & 0x00ff;	// Timer 1 low-order latches ('T1L-L').
		case 0x7:	return registers_.timer_latch[0] >> 8;		// Timer 1 high-order latches ('T1L-H').

		// Timer 2
		case 0x8:	// Timer 2 low-order counter ('T2C-L').
			registers_.interrupt_flags &= ~InterruptFlag::Timer2;
			reevaluate_interrupts();
		return registers_.timer[1] & 0x00ff;
		case 0x9:	return registers_.timer[1] >> 8;	// Timer 2 high-order counter ('T2C-H').

		case 0xa:	// Shift register ('SR').
			shift_bits_remaining_ = 8;
			registers_.interrupt_flags &= ~InterruptFlag::ShiftRegister;
			reevaluate_interrupts();
		return registers_.shift;

		case 0xb:	return registers_.auxiliary_control;	// Auxiliary control ('ACR').
		case 0xc:	return registers_.peripheral_control;	// Peripheral control ('PCR').

		case 0xd:	return registers_.interrupt_flags | (get_interrupt_line() ? 0x80 : 0x00);	// Interrupt flag register ('IFR').
		case 0xe:	return registers_.interrupt_enable | 0x80;									// Interrupt enable register ('IER').
	}

	return 0xff;
}

template <typename T> uint8_t MOS6522<T>::get_port_input(Port port, uint8_t output_mask, uint8_t output, uint8_t timer_mask) {
	bus_handler_.run_for(time_since_bus_handler_call_.flush<HalfCycles>());
	const uint8_t input = bus_handler_.get_port_input(port);
	output = (output & ~timer_mask) | (registers_.timer_port_b_output & timer_mask);
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

		bus_handler_.run_for(time_since_bus_handler_call_.flush<HalfCycles>());
		bus_handler_.set_interrupt_status(new_interrupt_status);
	}
}

template <typename T> void MOS6522<T>::set_control_line_input(Port port, Line line, bool value) {
	switch(line) {
		case Line::One:
			if(value != control_inputs_[port].lines[line]) {
				// In handshake mode, any transition on C[A/B]1 sets output high on C[A/B]2.
				if(handshake_modes_[port] == HandshakeMode::Handshake) {
					set_control_line_output(port, Line::Two, LineState::On);
				}

				// Set the proper transition interrupt bit if enabled.
				if(value == !!(registers_.peripheral_control & (port ? 0x10 : 0x01))) {
					registers_.interrupt_flags |= port ? InterruptFlag::CB1ActiveEdge : InterruptFlag::CA1ActiveEdge;
					reevaluate_interrupts();
				}

				// If this is a transition on CB1, consider updating the shift register.
				// TODO: and at least one full clock since the shift register was written?
				if(port == Port::B) {
					switch(shift_mode()) {
						default: 													break;
						case ShiftMode::InUnderCB1:		if(value)	shift_in();		break;	// Shifts in are captured on a low-to-high transition.
						case ShiftMode::OutUnderCB1:	if(!value)	shift_out();	break;	// Shifts out are updated on a high-to-low transition.
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
		// Decrement timer 1 based on clock if enabled.
		if(!(registers_.auxiliary_control & 0x20)) {
			-- registers_.timer[0];
		}
	}

	// Count down timer 2 if it is in timed interrupt mode (i.e. auxiliary control bit 5 is clear).
	// TODO: implement count down on PB6 if this bit isn't set.
	registers_.timer[1] -= 1 ^ ((registers_.auxiliary_control >> 5)&1);

	if(registers_.next_timer[0] >= 0) {
		registers_.timer[0] = uint16_t(registers_.next_timer[0]);
		registers_.next_timer[0] = -1;
	}
	if(registers_.next_timer[1] >= 0) {
		registers_.timer[1] = uint16_t(registers_.next_timer[1]);
		registers_.next_timer[1] = -1;
	}

	// In pulse modes, CA2 and CB2 go high again on the next clock edge.
	if(handshake_modes_[1] == HandshakeMode::Pulse) {
		set_control_line_output(Port::B, Line::Two, LineState::On);
	}
	if(handshake_modes_[0] == HandshakeMode::Pulse) {
		set_control_line_output(Port::A, Line::Two, LineState::On);
	}

	// If the shift register is shifting according to the input clock, do a shift.
	switch(shift_mode()) {
		default: 											break;
		case ShiftMode::InUnderPhase2:		shift_in();		break;
		case ShiftMode::OutUnderPhase2:		shift_out();	break;
	}
}

template <typename T> void MOS6522<T>::do_phase1() {
	++ time_since_bus_handler_call_;

	// IRQ is raised on the half cycle after overflow
	if((registers_.timer[1] == 0xffff) && !registers_.last_timer[1] && timer_is_running_[1]) {
		timer_is_running_[1] = false;

		// If the shift register is shifting according to this timer, do a shift.
		// TODO: "shift register is driven by only the low order 8 bits of timer 2"?
		switch(shift_mode()) {
			default: 												break;
			case ShiftMode::InUnderT2:				shift_in();		break;
			case ShiftMode::OutUnderT2FreeRunning: 	shift_out();	break;
			case ShiftMode::OutUnderT2:				shift_out();	break;	// TODO: present a clock on CB1.
		}

		registers_.interrupt_flags |= InterruptFlag::Timer2;
		reevaluate_interrupts();
	}

	if((registers_.timer[0] == 0xffff) && !registers_.last_timer[0] && timer_is_running_[0]) {
		registers_.interrupt_flags |= InterruptFlag::Timer1;
		reevaluate_interrupts();

		// Determine whether to reload.
		if(registers_.auxiliary_control&0x40)
			registers_.timer_needs_reload = true;
		else
			timer_is_running_[0] = false;

		// Determine whether to toggle PB7.
		if(registers_.auxiliary_control&0x80) {
			registers_.timer_port_b_output ^= 0x80;
			bus_handler_.run_for(time_since_bus_handler_call_.flush<HalfCycles>());
			evaluate_port_b_output();
		}
	}
}

template <typename T> void MOS6522<T>::evaluate_port_b_output() {
	// Apply current timer-linked PB7 output if any atop the stated output.
	const uint8_t timer_control_bit = registers_.auxiliary_control & 0x80;
	bus_handler_.set_port_output(
		Port::B,
		(registers_.output[1] & (0xff ^ timer_control_bit)) | timer_control_bit,
		registers_.data_direction[1] | timer_control_bit);
}

/*! Runs for a specified number of half cycles. */
template <typename T> void MOS6522<T>::run_for(const HalfCycles half_cycles) {
	auto number_of_half_cycles = half_cycles.as_integral();
	if(!number_of_half_cycles) return;

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
	bus_handler_.run_for(time_since_bus_handler_call_.flush<HalfCycles>());
	bus_handler_.flush();
}

/*! Runs for a specified number of cycles. */
template <typename T> void MOS6522<T>::run_for(const Cycles cycles) {
	auto number_of_cycles = cycles.as_integral();
	while(number_of_cycles--) {
		do_phase1();
		do_phase2();
	}
}

/*! @returns @c true if the IRQ line is currently active; @c false otherwise. */
template <typename T> bool MOS6522<T>::get_interrupt_line() const {
	uint8_t interrupt_status = registers_.interrupt_flags & registers_.interrupt_enable & 0x7f;
	return interrupt_status;
}

template <typename T> void MOS6522<T>::evaluate_cb2_output() {
	// CB2 is a special case, being both the line the shift register can output to,
	// and one that can be used as an input or handshaking output according to the
	// peripheral control register.

	// My guess: other CB2 functions work only if the shift register is disabled (?).
	if(shift_mode() != ShiftMode::Disabled) {
		// Shift register is enabled, one way or the other; but announce only output.
		if(is_shifting_out()) {
			// Output mode; set the level according to the current top of the shift register.
			bus_handler_.set_control_line_output(Port::B, Line::Two, !!(registers_.shift & 0x80));
		} else {
			// Input mode.
			bus_handler_.set_control_line_output(Port::B, Line::Two, true);
		}
	} else {
		// Shift register is disabled.
		bus_handler_.set_control_line_output(Port::B, Line::Two, control_outputs_[1].lines[1] != LineState::Off);
	}
}

template <typename T> void MOS6522<T>::set_control_line_output(Port port, Line line, LineState value) {
	if(port == Port::B && line == Line::Two) {
		control_outputs_[port].lines[line] = value;
		evaluate_cb2_output();
	} else {
		// Do nothing if unchanged.
		if(value == control_outputs_[port].lines[line]) {
			return;
		}

		control_outputs_[port].lines[line] = value;

		if(value != LineState::Input) {
			bus_handler_.run_for(time_since_bus_handler_call_.flush<HalfCycles>());
			bus_handler_.set_control_line_output(port, line, value != LineState::Off);
		}
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
	// When shifting out, the shift register rotates rather than strictly shifts.
	// TODO: is that true for all modes?
	if(shift_mode() == ShiftMode::OutUnderT2FreeRunning || shift_bits_remaining_) {
		registers_.shift = uint8_t((registers_.shift << 1) | (registers_.shift >> 7));
		evaluate_cb2_output();

		--shift_bits_remaining_;
		if(!shift_bits_remaining_) {
			registers_.interrupt_flags |= InterruptFlag::ShiftRegister;
			reevaluate_interrupts();
		}
	}
}

}
}
