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
				case 0x00:
				case 0x02:
					static_cast<T *>(this)->set_port_output(decodedAddress / 2, value, _port[decodedAddress / 2].direction);
					_port[decodedAddress/2].output = value;
				break;

				case 0x01:
				case 0x03:
					_port[decodedAddress / 2].direction = value;
				break;

				case 0x04:
				case 0x05:
				case 0x06:
				case 0x07:
					_timer.writtenShift = _timer.activeShift = (decodedAddress - 0x04) * 3 + (decodedAddress / 0x07);	// i.e. 0, 3, 6, 10
					_timer.value = ((unsigned int)(value) << _timer.activeShift) | ((1 << _timer.activeShift)-1);
					_timer.status &= ~0x80;
				break;
			}
		}

		inline uint8_t get_register(int address)
		{
			const uint8_t decodedAddress = address & 0x7;
			switch(decodedAddress) {
				case 0x00:
				case 0x02:
				{
					const int port = decodedAddress / 2;
					uint8_t input = static_cast<T *>(this)->get_port_input(port);
					return (input & ~_port[port].direction) | (_port[port].output & _port[port].direction);
				}
				break;
				case 0x01:
				case 0x03:
					return _port[decodedAddress / 2].direction;
				break;
				case 0x04:
				case 0x06:
				{
					uint8_t value = (uint8_t)(_timer.value >> _timer.activeShift);

					if(_timer.activeShift != _timer.writtenShift) {
						unsigned int shift = _timer.writtenShift - _timer.activeShift;
						_timer.value = (_timer.value << shift) | ((1 << shift) - 1);
						_timer.activeShift = _timer.writtenShift;
					}

					return value;
				}
				break;
				case 0x05:
				case 0x07:
				{
					uint8_t value = _timer.status;
					_timer.status &= ~0x40;
					return value;
				}
				break;
			}

			return 0xff;
		}

		inline void run_for_cycles(unsigned int number_of_cycles)
		{
			if(_timer.value >= number_of_cycles) {
				_timer.value -= number_of_cycles;
			} else {
				number_of_cycles -= _timer.value;
				_timer.value = 0x100 - number_of_cycles;
				_timer.activeShift = 0;
				_timer.status |= 0xc0;
			}
		}

		MOS6532() :
			_timer({.status = 0})
		{}

	private:
		uint8_t _ram[128];

		struct {
			unsigned int value;
			unsigned int activeShift, writtenShift;
			uint8_t status;
		} _timer;

		struct {
			uint8_t direction, output;
		} _port[2];

		// expected to be overridden
		uint8_t get_port_input(int port)										{	return 0xff;	}
		void set_port_output(int port, uint8_t value, uint8_t direction_mask)	{}
};

}

#endif /* _532_hpp */
