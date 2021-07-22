//
//  6526.h
//  Clock Signal
//
//  Created by Thomas Harte on 18/07/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef _526_h
#define _526_h

#include <cstdint>

#include "Implementation/6526Storage.hpp"

namespace MOS {
namespace MOS6526 {

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
	private MOS6526Storage
{
	public:
		MOS6526(PortHandlerT &port_handler) noexcept : port_handler_(port_handler) {}
		MOS6526(const MOS6526 &) = delete;

		/// Writes @c value to the register at @c address. Only the low two bits of the address are decoded.
		void write(int address, uint8_t value);

		/// Fetches the value of the register @c address. Only the low two bits of the address are decoded.
		uint8_t read(int address);

		/// Runs for a specified number of half cycles.
		void run_for(const HalfCycles half_cycles);

	private:
		PortHandlerT &port_handler_;

		template <int port> void set_port_output();
		template <int port> uint8_t get_port_input();
		void update_interrupts();
};

}
}

#include "Implementation/6526Implementation.hpp"

#endif /* _526_h */
