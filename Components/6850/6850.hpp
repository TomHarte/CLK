//
//  6850.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/10/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef Motorola_ACIA_6850_hpp
#define Motorola_ACIA_6850_hpp

#include <cstdint>
#include "../../ClockReceiver/ClockReceiver.hpp"
#include "../../ClockReceiver/ForceInline.hpp"
#include "../../ClockReceiver/ClockingHintSource.hpp"
#include "../Serial/Line.hpp"

namespace Motorola {
namespace ACIA {

class ACIA: public ClockingHint::Source, private Serial::Line::ReadDelegate {
	public:
		static constexpr const HalfCycles SameAsTransmit = HalfCycles(0);

		/*!
			Constructs a new instance of ACIA which will receive a transmission clock at a rate of
			@c transmit_clock_rate, and a receive clock at a rate of @c receive_clock_rate.
		*/
		ACIA(HalfCycles transmit_clock_rate, HalfCycles receive_clock_rate = SameAsTransmit);

		/*!
			Reads from the ACIA.

			Bit 0 of the address is used as the ACIA's register select line —
			so even addresses select control/status registers, odd addresses
			select transmit/receive data registers.
		*/
		uint8_t read(int address);

		/*!
			Writes to the ACIA.

			Bit 0 of the address is used as the ACIA's register select line —
			so even addresses select control/status registers, odd addresses
			select transmit/receive data registers.
		*/
		void write(int address, uint8_t value);

		/*!
			Advances @c transmission_cycles in time, which should be
			counted relative to the @c transmit_clock_rate.
		*/
		forceinline void run_for(HalfCycles transmission_cycles) {
			if(transmit.transmission_data_time_remaining() > HalfCycles(0)) {
				const auto write_data_time_remaining = transmit.write_data_time_remaining();

				// There's at most one further byte available to enqueue, so a single 'if'
				// rather than a 'while' is correct here. It's the responsibilit of the caller
				// to ensure run_for lengths are appropriate for longer sequences.
				if(transmission_cycles >= write_data_time_remaining) {
					if(next_transmission_ != NoValueMask) {
						transmit.advance_writer(write_data_time_remaining);
						consider_transmission();
						transmit.advance_writer(transmission_cycles - write_data_time_remaining);
					} else {
						transmit.advance_writer(transmission_cycles);
						update_clocking_observer();
						update_interrupt_line();
					}
				} else {
					transmit.advance_writer(transmission_cycles);
				}
			}
		}

		bool get_interrupt_line() const;
		void reset();

		// Input lines.
		Serial::Line receive;
		Serial::Line clear_to_send;
		Serial::Line data_carrier_detect;

		// Output lines.
		Serial::Line transmit;
		Serial::Line request_to_send;

		// ClockingHint::Source.
		ClockingHint::Preference preferred_clocking() const final;

		struct InterruptDelegate {
			virtual void acia6850_did_change_interrupt_status(ACIA *acia) = 0;
		};
		void set_interrupt_delegate(InterruptDelegate *delegate);

	private:
		int divider_ = 1;
		enum class Parity {
			Even, Odd, None
		} parity_ = Parity::None;
		int data_bits_ = 7, stop_bits_ = 2;

		static constexpr int NoValueMask = 0x100;
		int next_transmission_ = NoValueMask;
		int received_data_ = NoValueMask;

		int bits_received_ = 0;
		int bits_incoming_ = 0;
		bool overran_ = false;

		void consider_transmission();
		int expected_bits();
		uint8_t parity(uint8_t value);

		bool receive_interrupt_enabled_ = false;
		bool transmit_interrupt_enabled_ = false;

		HalfCycles transmit_clock_rate_;
		HalfCycles receive_clock_rate_;

		bool serial_line_did_produce_bit(Serial::Line *line, int bit) final;

		bool interrupt_line_ = false;
		void update_interrupt_line();
		InterruptDelegate *interrupt_delegate_ = nullptr;
		uint8_t get_status();
};

}
}

#endif /* Motorola_ACIA_6850_hpp */
