//
//  6532.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 19/06/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#ifndef _532_hpp
#define _532_hpp

#include <cstdint>
#include <cstdio>

#include "../../ClockReceiver/ClockReceiver.hpp"

namespace MOS {

/*!
	Implements a template for emulation of the MOS 6532 RAM-I/O-Timer ('RIOT').

	The RIOT provides:
		* 128 bytes of static RAM;
		* an interval timer; and
		* two digital input/output ports.

	Consumers should derive their own curiously-recurring-template-pattern subclass,
	implementing bus communications as required.
*/
template <class T> class MOS6532 {
	public:
		inline void set_ram(uint16_t address, uint8_t value)	{	ram_[address&0x7f] = value;		}
		inline uint8_t get_ram(uint16_t address)				{	return ram_[address & 0x7f];	}

		inline void write(int address, uint8_t value) {
			const uint8_t decodedAddress = address & 0x07;
			switch(decodedAddress) {
				// Port output
				case 0x00: case 0x02:
					port_[decodedAddress / 2].output = value;
					static_cast<T *>(this)->set_port_output(decodedAddress / 2, port_[decodedAddress/2].output, port_[decodedAddress / 2].output_mask);
					set_port_did_change(decodedAddress / 2);
				break;
				case 0x01: case 0x03:
					port_[decodedAddress / 2].output_mask = value;
					static_cast<T *>(this)->set_port_output(decodedAddress / 2, port_[decodedAddress/2].output, port_[decodedAddress / 2].output_mask);
					set_port_did_change(decodedAddress / 2);
				break;

				// The timer and edge detect control
				case 0x04: case 0x05: case 0x06: case 0x07:
					if(address & 0x10) {
						timer_.writtenShift = timer_.activeShift = (decodedAddress - 0x04) * 3 + (decodedAddress / 0x07);	// i.e. 0, 3, 6, 10
						timer_.value = (static_cast<unsigned int>(value) << timer_.activeShift) ;
						timer_.interrupt_enabled = !!(address&0x08);
						interrupt_status_ &= ~InterruptFlag::Timer;
						evaluate_interrupts();
					} else {
						a7_interrupt_.enabled = !!(address&0x2);
						a7_interrupt_.active_on_positive = !!(address & 0x01);
					}
				break;
			}
		}

		inline uint8_t read(int address) {
			const uint8_t decodedAddress = address & 0x7;
			switch(decodedAddress) {
				// Port input
				case 0x00: case 0x02: {
					const int port = decodedAddress / 2;
					uint8_t input = static_cast<T *>(this)->get_port_input(port);
					return (input & ~port_[port].output_mask) | (port_[port].output & port_[port].output_mask);
				}
				break;
				case 0x01: case 0x03:
					return port_[decodedAddress / 2].output_mask;
				break;

				// Timer and interrupt control
				case 0x04: case 0x06: {
					uint8_t value = static_cast<uint8_t>(timer_.value >> timer_.activeShift);
					timer_.interrupt_enabled = !!(address&0x08);
					interrupt_status_ &= ~InterruptFlag::Timer;
					evaluate_interrupts();

					if(timer_.activeShift != timer_.writtenShift) {
						unsigned int shift = timer_.writtenShift - timer_.activeShift;
						timer_.value = (timer_.value << shift) | ((1 << shift) - 1);
						timer_.activeShift = timer_.writtenShift;
					}

					return value;
				}
				break;

				case 0x05: case 0x07: {
					uint8_t value = interrupt_status_;
					interrupt_status_ &= ~InterruptFlag::PA7;
					evaluate_interrupts();
					return value;
				}
				break;
			}

			return 0xff;
		}

		inline void run_for(const Cycles cycles) {
			unsigned int number_of_cycles = static_cast<unsigned int>(cycles.as_integral());

			// permit counting _to_ zero; counting _through_ zero initiates the other behaviour
			if(timer_.value >= number_of_cycles) {
				timer_.value -= number_of_cycles;
			} else {
				number_of_cycles -= timer_.value;
				timer_.value = (0x100 - number_of_cycles) & 0xff;
				timer_.activeShift = 0;
				interrupt_status_ |= InterruptFlag::Timer;
				evaluate_interrupts();
			}
		}

		MOS6532() {
			timer_.value = static_cast<unsigned int>((rand() & 0xff) << 10);
		}

		inline void set_port_did_change(int port) {
			if(!port) {
				uint8_t new_port_a_value = (get_port_input(0) & ~port_[0].output_mask) | (port_[0].output & port_[0].output_mask);
				uint8_t difference = new_port_a_value ^ a7_interrupt_.last_port_value;
				a7_interrupt_.last_port_value = new_port_a_value;
				if(difference&0x80) {
					if(
						((new_port_a_value&0x80) && a7_interrupt_.active_on_positive) ||
						(!(new_port_a_value&0x80) && !a7_interrupt_.active_on_positive)
					) {
						interrupt_status_ |= InterruptFlag::PA7;
						evaluate_interrupts();
					}
				}
			}
		}

		inline bool get_inerrupt_line() const {
			return interrupt_line_;
		}

	private:
		uint8_t ram_[128];

		struct {
			unsigned int value;
			unsigned int activeShift = 10, writtenShift = 10;
			bool interrupt_enabled = false;
		} timer_;

		struct {
			bool enabled = false;
			bool active_on_positive = false;
			uint8_t last_port_value = 0;
		} a7_interrupt_;

		struct {
			uint8_t output_mask = 0, output = 0;
		} port_[2];

		uint8_t interrupt_status_ = 0;
		enum InterruptFlag: uint8_t {
			Timer = 0x80,
			PA7 = 0x40
		};
		bool interrupt_line_ = false;

		// expected to be overridden
		uint8_t get_port_input(int port)										{	return 0xff;	}
		void set_port_output(int port, uint8_t value, uint8_t output_mask)		{}
		void set_irq_line(bool new_value)										{}

		inline void evaluate_interrupts() {
			interrupt_line_ =
				((interrupt_status_&InterruptFlag::Timer) && timer_.interrupt_enabled) ||
				((interrupt_status_&InterruptFlag::PA7) && a7_interrupt_.enabled);
			set_irq_line(interrupt_line_);
		}
};

}

#endif /* _532_hpp */
