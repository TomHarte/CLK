//
//  6526Implementation.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/07/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#pragma once

#include <cassert>
#include <cstdio>

namespace MOS::MOS6526 {

enum Interrupts: uint8_t {
	TimerA = 1 << 0,
	TimerB = 1 << 1,
	Alarm = 1 << 2,
	SerialPort = 1 << 3,
	Flag = 1 << 4,
};

template <typename BusHandlerT, Personality personality>
template <int port> void MOS6526<BusHandlerT, personality>::set_port_output() {
	const uint8_t output = output_[port] | (~data_direction_[port]);
	port_handler_.set_port_output(Port(port), output);
}

template <typename BusHandlerT, Personality personality>
template <int port> uint8_t MOS6526<BusHandlerT, personality>::get_port_input() {
	// Avoid bothering the port handler if there's no input active.
	const uint8_t input_mask = ~data_direction_[port];
	const uint8_t input = input_mask ? port_handler_.get_port_input(Port(port)) : 0x00;
	return (input & input_mask) | (output_[port] & data_direction_[port]);
}

template <typename BusHandlerT, Personality personality>
void MOS6526<BusHandlerT, personality>::posit_interrupt(uint8_t mask) {
	if(!mask) {
		return;
	}
	interrupt_state_ |= mask;
	update_interrupts();
}

template <typename BusHandlerT, Personality personality>
void MOS6526<BusHandlerT, personality>::update_interrupts() {
	if(interrupt_state_ & interrupt_control_) {
		pending_ |= InterruptInOne;
	}
}

template <typename BusHandlerT, Personality personality>
bool MOS6526<BusHandlerT, personality>::get_interrupt_line() {
	return interrupt_state_ & 0x80;
}

template <typename BusHandlerT, Personality personality>
void MOS6526<BusHandlerT, personality>::set_cnt_input(bool active) {
	cnt_edge_ = active && !cnt_state_;
	cnt_state_ = active;
}

template <typename BusHandlerT, Personality personality>
void MOS6526<BusHandlerT, personality>::set_flag_input(bool low) {
	if(low && !flag_state_) {
		posit_interrupt(Interrupts::Flag);
	}
	flag_state_ = low;
}

template <typename BusHandlerT, Personality personality>
void MOS6526<BusHandlerT, personality>::write(int address, uint8_t value) {
	address &= 0xf;
	switch(address) {
		// Port output.
		case 0:
			output_[0] = value;
			set_port_output<0>();
		break;
		case 1:
			output_[1] = value;
			set_port_output<1>();
		break;

		// Port direction.
		case 2:
			data_direction_[0] = value;
			set_port_output<0>();
		break;
		case 3:
			data_direction_[1] = value;
			set_port_output<1>();
		break;

		// Counters; writes set the reload values.
		case 4:		counter_[0].template set_reload<0, personality == Personality::P8250>(value);	break;
		case 5:		counter_[0].template set_reload<8, personality == Personality::P8250>(value);	break;
		case 6:		counter_[1].template set_reload<0, personality == Personality::P8250>(value);	break;
		case 7:		counter_[1].template set_reload<8, personality == Personality::P8250>(value);	break;

		// Time-of-day clock.
		case 8:		tod_.template write<0>(value);	break;
		case 9:		tod_.template write<1>(value);	break;
		case 10:	tod_.template write<2>(value);	break;
		case 11:	tod_.template write<3>(value);	break;

		// Interrupt control.
		case 13: {
			if(value & 0x80) {
				interrupt_control_ |= value & 0x7f;
			} else {
				interrupt_control_ &= ~(value & 0x7f);
			}
			update_interrupts();
		} break;

		// Control. Posted to both the counters and the clock as it affects both.
		case 14:
			counter_[0].template set_control<false>(value);
			tod_.template set_control<false>(value);
			if(shifter_is_output_ != bool(value & 0x40)) {
				shifter_is_output_ = value & 0x40;
				shift_bits_ = 0;
			}
		break;
		case 15:
			counter_[1].template set_control<true>(value);
			tod_.template set_control<true>(value);
		break;

		// Shift control.
		case 12:
			printf("TODO: write to shift register\n");
		break;

		default:
			printf("Unhandled 6526 write: %02x to %d\n", value, address);
			assert(false);
		break;
	}
}

template <typename BusHandlerT, Personality personality>
uint8_t MOS6526<BusHandlerT, personality>::read(int address) {
	address &= 0xf;
	switch(address) {
		case 0:		return get_port_input<0>();
		case 1:		return get_port_input<1>();

		case 2: case 3:
		return data_direction_[address - 2];

		// Counters; reads obtain the current values.
		case 4:	return uint8_t(counter_[0].value >> 0);
		case 5:	return uint8_t(counter_[0].value >> 8);
		case 6:	return uint8_t(counter_[1].value >> 0);
		case 7:	return uint8_t(counter_[1].value >> 8);

		// Interrupt state.
		case 13: {
			const uint8_t result = interrupt_state_;
			interrupt_state_ = 0;
			pending_ &= ~(InterruptNow | InterruptInOne);
			update_interrupts();
			return result;
		} break;

		case 14: case 15:
		return counter_[address - 14].control;

		// Time-of-day clock.
		case 8:		return tod_.template read<0>();
		case 9:		return tod_.template read<1>();
		case 10:	return tod_.template read<2>();
		case 11:	return tod_.template read<3>();

		// Shift register.
		case 12:	return shift_data_;

		default:
			printf("Unhandled 6526 read from %d\n", address);
			assert(false);
		break;
	}
	return 0xff;
}

template <typename BusHandlerT, Personality personality>
void MOS6526<BusHandlerT, personality>::run_for(const HalfCycles half_cycles) {
	half_divider_ += half_cycles;
	int sub = half_divider_.divide_cycles().template as<int>();

	while(sub--) {
		pending_ <<= 1;
		if(pending_ & InterruptNow) {
			interrupt_state_ |= 0x80;
		}
		pending_ &= PendingClearMask;

		// TODO: use CNT potentially to clock timer A, elimiante conditional above.
		const bool timer1_did_reload = counter_[0].template advance<false>(false, cnt_state_, cnt_edge_);

		const bool timer1_carry = timer1_did_reload && (counter_[1].control & 0x60) == 0x40;
		const bool timer2_did_reload = counter_[1].template advance<true>(timer1_carry, cnt_state_, cnt_edge_);
		posit_interrupt((timer1_did_reload ? Interrupts::TimerA : 0x00) | (timer2_did_reload ? Interrupts::TimerB : 0x00));

		cnt_edge_ = false;
	}
}

template <typename BusHandlerT, Personality personality>
void MOS6526<BusHandlerT, personality>::advance_tod(int count) {
	if(!count) return;
	if(tod_.advance(count)) {
		posit_interrupt(Interrupts::Alarm);
	}
}

template <typename BusHandlerT, Personality personality>
bool MOS6526<BusHandlerT, personality>::serial_line_did_produce_bit(Serial::Line<true> *, int bit) {
	// TODO: post CNT change; might affect timer.

	if(!shifter_is_output_) {
		shift_register_ = uint8_t((shift_register_ << 1) | bit);
		++shift_bits_;

		if(shift_bits_ == 8) {
			shift_bits_ = 0;
			shift_data_ = shift_register_;
			posit_interrupt(Interrupts::SerialPort);
		}
	}

	return true;
}

}
