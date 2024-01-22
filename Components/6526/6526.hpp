//
//  6526.h
//  Clock Signal
//
//  Created by Thomas Harte on 18/07/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#pragma once

#include <cstdint>

#include "Implementation/6526Storage.hpp"
#include "../Serial/Line.hpp"

namespace MOS::MOS6526 {

enum Port {
	A = 0,
	B = 1
};

struct PortHandler {
	/// Requests the current input value of @c port from the port handler.
	uint8_t get_port_input([[maybe_unused]] Port port) {
		return 0xff;
	}

	/// Sets the current output value of @c port; any bits marked as input will be supplied as 1s.
	void set_port_output([[maybe_unused]] Port port, [[maybe_unused]] uint8_t value) {}
};

enum class Personality {
	// The 6526, used in machines such as the C64, has a BCD time-of-day clock.
	P6526,
	// The 8250, used in the Amiga, provides a binary time-of-day clock.
	P8250,
};

template <typename PortHandlerT, Personality personality> class MOS6526:
	private MOS6526Storage,
	private Serial::Line<true>::ReadDelegate
{
	public:
		MOS6526(PortHandlerT &port_handler) noexcept : port_handler_(port_handler) {
			serial_input.set_read_delegate(this);
		}
		MOS6526(const MOS6526 &) = delete;

		/// Writes @c value to the register at @c address. Only the low two bits of the address are decoded.
		void write(int address, uint8_t value);

		/// Fetches the value of the register @c address. Only the low two bits of the address are decoded.
		uint8_t read(int address);

		/// Pulses Phi2 to advance by the specified number of half cycles.
		void run_for(const HalfCycles half_cycles);

		/// Pulses the TOD input the specified number of times.
		void advance_tod(int count);

		/// @returns @c true if the interrupt output is active, @c false otherwise.
		bool get_interrupt_line();

		/// Sets the current state of the CNT input.
		void set_cnt_input(bool active);

		/// Provides both the serial input bit and an additional source of CNT.
		Serial::Line<true> serial_input;

		/// Sets the current state of the FLG input.
		void set_flag_input(bool low);

	private:
		PortHandlerT &port_handler_;
		TODStorage<personality == Personality::P8250> tod_;

		template <int port> void set_port_output();
		template <int port> uint8_t get_port_input();
		void update_interrupts();
		void posit_interrupt(uint8_t mask);
		void advance_counters(int);

		bool serial_line_did_produce_bit(Serial::Line<true> *line, int bit) final;
};

}

#include "Implementation/6526Implementation.hpp"
