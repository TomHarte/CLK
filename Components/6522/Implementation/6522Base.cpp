//
//  6522Base.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/09/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "../6522.hpp"

using namespace MOS::MOS6522;

void MOS6522Base::set_control_line_input(Port port, Line line, bool value) {
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

void MOS6522Base::do_phase2() {
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

void MOS6522Base::do_phase1() {
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
void MOS6522Base::run_for(const HalfCycles half_cycles) {
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
void MOS6522Base::run_for(const Cycles cycles) {
	int number_of_cycles = cycles.as_int();
	while(number_of_cycles--) {
		do_phase1();
		do_phase2();
	}
}

/*! @returns @c true if the IRQ line is currently active; @c false otherwise. */
bool MOS6522Base::get_interrupt_line() {
	uint8_t interrupt_status = registers_.interrupt_flags & registers_.interrupt_enable & 0x7f;
	return !!interrupt_status;
}
