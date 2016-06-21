//
//  6532.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 19/06/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef _532_hpp
#define _532_hpp

#include <cstdint>
#include <cstdio>

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
		inline void set_ram(uint16_t address, uint8_t value)	{	_ram[address&0x7f] = value;		}
		inline uint8_t get_ram(uint16_t address)				{	return _ram[address & 0x7f];	}

		inline void set_register(int address, uint8_t value)
		{
			const uint8_t decodedAddress = address & 0x07;
			switch(decodedAddress) {
				// Port output
				case 0x00: case 0x02:
					_port[decodedAddress / 2].output = value;
					static_cast<T *>(this)->set_port_output(decodedAddress / 2, _port[decodedAddress/2].output, _port[decodedAddress / 2].output_mask);
					set_port_did_change(decodedAddress / 2);
				break;
				case 0x01: case 0x03:
					_port[decodedAddress / 2].output_mask = value;
					static_cast<T *>(this)->set_port_output(decodedAddress / 2, _port[decodedAddress/2].output, _port[decodedAddress / 2].output_mask);
					set_port_did_change(decodedAddress / 2);
				break;

				// The timer and edge detect control
				case 0x04: case 0x05: case 0x06: case 0x07:
					if(address & 0x10)
					{
						_timer.writtenShift = _timer.activeShift = (decodedAddress - 0x04) * 3 + (decodedAddress / 0x07);	// i.e. 0, 3, 6, 10
						_timer.value = ((unsigned int)(value) << _timer.activeShift) | ((1 << _timer.activeShift)-1);
						_timer.interrupt_enabled = !!(address&0x08);
						_interrupt_status &= ~InterruptFlag::Timer;
						evaluate_interrupts();
					}
					else
					{
						_a7_interrupt.enabled = !!(address&0x2);
						_a7_interrupt.active_on_positive = !!(address & 0x01);
					}
				break;
			}
		}

		inline uint8_t get_register(int address)
		{
			const uint8_t decodedAddress = address & 0x7;
			switch(decodedAddress) {
				// Port input
				case 0x00: case 0x02:
				{
					const int port = decodedAddress / 2;
					uint8_t input = static_cast<T *>(this)->get_port_input(port);
					return (input & ~_port[port].output_mask) | (_port[port].output & _port[port].output_mask);
				}
				break;
				case 0x01: case 0x03:
					return _port[decodedAddress / 2].output_mask;
				break;

				// Timer and interrupt control
				case 0x04: case 0x06:
				{
					uint8_t value = (uint8_t)(_timer.value >> _timer.activeShift);
					_timer.interrupt_enabled = !!(address&0x08);
					_interrupt_status &= ~InterruptFlag::Timer;
					evaluate_interrupts();

					if(_timer.activeShift != _timer.writtenShift) {
						unsigned int shift = _timer.writtenShift - _timer.activeShift;
						_timer.value = (_timer.value << shift) | ((1 << shift) - 1);
						_timer.activeShift = _timer.writtenShift;
					}

					return value;
				}
				break;

				case 0x05: case 0x07:
				{
					uint8_t value = _interrupt_status;
					_interrupt_status &= ~InterruptFlag::PA7;
					evaluate_interrupts();
					return value;
				}
				break;
			}

			return 0xff;
		}

		inline void run_for_cycles(unsigned int number_of_cycles)
		{
			// permit counting _to_ zero; counting _through_ zero initiates the other behaviour
			if(_timer.value >= number_of_cycles) {
				_timer.value -= number_of_cycles;
			} else {
				number_of_cycles -= _timer.value;
				_timer.value = 0x100 - number_of_cycles;
				_timer.activeShift = 0;
				_interrupt_status |= InterruptFlag::Timer;
				evaluate_interrupts();
			}
		}

		MOS6532() :
			_interrupt_status(0),
			_port{{.output_mask = 0, .output = 0}, {.output_mask = 0, .output = 0}},
			_a7_interrupt({.last_port_value = 0, .enabled = false}),
			_interrupt_line(false)
		{}

		inline void set_port_did_change(int port)
		{
			if(!port)
			{
				uint8_t new_port_a_value = (get_port_input(0) & ~_port[0].output_mask) | (_port[0].output & _port[0].output_mask);
				uint8_t difference = new_port_a_value ^ _a7_interrupt.last_port_value;
				_a7_interrupt.last_port_value = new_port_a_value;
				if(difference&0x80)
				{
					if(
						((new_port_a_value&0x80) && _a7_interrupt.active_on_positive) ||
						(!(new_port_a_value&0x80) && !_a7_interrupt.active_on_positive)
					)
					{
						_interrupt_status |= InterruptFlag::PA7;
						evaluate_interrupts();
					}
				}
			}
		}

		inline bool get_inerrupt_line()
		{
			return _interrupt_line;
		}

	private:
		uint8_t _ram[128];

		struct {
			unsigned int value;
			unsigned int activeShift, writtenShift;
			bool interrupt_enabled;
		} _timer;

		struct {
			bool enabled;
			bool active_on_positive;
			uint8_t last_port_value;
		} _a7_interrupt;

		struct {
			uint8_t output_mask, output;
		} _port[2];

		uint8_t _interrupt_status;
		enum InterruptFlag: uint8_t {
			Timer = 0x80,
			PA7 = 0x40
		};
		bool _interrupt_line;

		// expected to be overridden
		uint8_t get_port_input(int port)										{	return 0xff;	}
		void set_port_output(int port, uint8_t value, uint8_t output_mask)		{}
		void set_irq_line(bool new_value)										{}

		inline void evaluate_interrupts()
		{
			_interrupt_line =
				((_interrupt_status&InterruptFlag::Timer) && _timer.interrupt_enabled) ||
				((_interrupt_status&InterruptFlag::PA7) && _a7_interrupt.enabled);
			set_irq_line(_interrupt_line);
		}
};

}

#endif /* _532_hpp */
