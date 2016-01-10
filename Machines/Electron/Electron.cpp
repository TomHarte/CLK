//
//  Electron.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Electron.hpp"

#include <algorithm>

using namespace Electron;

static const int cycles_per_line = 128;
static const int cycles_per_frame = 312*cycles_per_line;
static const int crt_cycles_multiplier = 8;
static const int crt_cycles_per_line = crt_cycles_multiplier * cycles_per_line;

Machine::Machine() :
	_interruptControl(0),
	_frameCycles(0),
	_outputPosition(0)
{
	_crt = new Outputs::CRT(crt_cycles_per_line, 312, 1, 1);
	_interruptStatus = 0x02;
	setup6502();
}

Machine::~Machine()
{
}

unsigned int Machine::perform_bus_operation(CPU6502::BusOperation operation, uint16_t address, uint8_t *value)
{
	unsigned int cycles = 1;

	if(address < 32768)
	{
		if(isReadOperation(operation))
		{
			*value = _ram[address];
		}
		else
		{
			_ram[address] = *value;

// TODO: range check on address; a lot of the time the machine will be running code outside of
// the screen area, meaning that no update is required.
//			if (address
			update_display();
		}

		// TODO: RAM timing for Modes 0–3
		cycles += (_frameCycles&1)^1;
	}
	else
	{
		if(address > 49152)
		{
			if((address & 0xff00) == 0xfe00)
			{
//				printf("%c: %02x: ", isReadOperation(operation) ? 'r' : 'w', *value);

				switch(address&0xf)
				{
					case 0x0:
						if(isReadOperation(operation))
						{
							*value = _interruptStatus;
							_interruptStatus &= ~0x02;
						}
						else
						{
							_interruptControl = *value;
							evaluate_interrupts();
						}
					break;
					case 0x1:
					break;
					case 0x2:
						_startScreenAddress = (_startScreenAddress & 0xff00) | ((*value) & 0xe0);
					break;
					case 0x3:
						_startScreenAddress = (_startScreenAddress & 0x00ff) | (uint16_t)(((*value) & 0x3f) << 8);
					break;
					case 0x4:
						printf("Cassette\n");
					break;
					case 0x5:
						if(!isReadOperation(operation))
						{
							uint8_t nextROM = (*value)&0xf;
							if((_activeRom&0x12) != 0x8 || nextROM >= 8)
							{
								_activeRom = (Electron::ROMSlot)nextROM;
							}

							if( (*value)&0x10 ) _interruptStatus &= ~InterruptDisplayEnd;
							if( (*value)&0x20 ) _interruptStatus &= InterruptRealTimeClock;
							if( (*value)&0x40 ) _interruptStatus &= InterruptHighToneDetect;
							evaluate_interrupts();

							// TODO: NMI (?)
						}
					break;
					case 0x6:
						printf("Counter\n");
					break;
					case 0x7:
						printf("Misc. control\n");
					break;
					default:
						update_display();
//						printf("Palette\n");
					break;
				}
			}
			else
			{
				if(isReadOperation(operation))
					*value = _os[address - 49152];
			}
		}
		else
		{
			if(isReadOperation(operation))
			{
				switch(_activeRom)
				{
					case ROMSlotBASIC:
					case ROMSlotBASIC+1:
						*value = _basic[address - 32768];
					break;
					case ROMSlotKeyboard:
					case ROMSlotKeyboard+1:
						*value = 0;
					break;
					default: break;
				}
			}
		}
	}

	_frameCycles += cycles;
	if(_frameCycles == cycles_per_frame)
	{
		update_display();
		_frameCycles = 0;
	}
	if(_frameCycles == 128*128) signal_interrupt(InterruptRealTimeClock);
	if(_frameCycles == 284*128) signal_interrupt(InterruptDisplayEnd);

	return cycles;
}

void Machine::set_rom(ROMSlot slot, size_t length, const uint8_t *data)
{
	uint8_t *target = nullptr;
	switch(slot)
	{
		case ROMSlotBASIC:	target = _basic;	break;
		case ROMSlotOS:		target = _os;		break;
		default: return;
	}

	memcpy(target, data, std::min((size_t)16384, length));
}

inline void Machine::signal_interrupt(Electron::Interrupt interrupt)
{
	_interruptStatus |= (interrupt << 2);
	evaluate_interrupts();
}

inline void Machine::evaluate_interrupts()
{
	if(_interruptStatus & _interruptControl)
	{
		_interruptStatus |= 1;
	}
	set_irq_line(_interruptStatus & 1);
}

inline void Machine::update_display()
{
	const int end_of_hsync = 3 * cycles_per_line;

	if(_frameCycles >= end_of_hsync)
	{
		// assert sync for the first three lines of the display
		if(_outputPosition < end_of_hsync)
		{
			_crt->output_sync(end_of_hsync * crt_cycles_multiplier);
			_outputPosition = end_of_hsync;
		}

		while(_outputPosition < _frameCycles)
		{
			const int current_line = _outputPosition >> 7;
			const int line_position = _outputPosition & 127;

			// all lines then start with 9 cycles of sync
			if(!line_position)
			{
				_crt->output_sync(9 * crt_cycles_multiplier);
				_outputPosition += 9;
			}
			else
			{
				// on lines prior to 28 or after or equal to 284, or on a line that is equal to 8 or 9 modulo 10 in a line-spaced mode,
				// the line is then definitely blank.
				if(current_line < 28 || current_line >= 284)
				{
					if(line_position == 9)
					{
						_crt->output_blank(119 * crt_cycles_multiplier);
						_outputPosition += 119;
					}
				}
				else
				{
					// there are then 15 cycles of blank, 80 cycles of pixels, and 24 further cycles of blank
					if(line_position == 9)
					{
						_crt->output_blank(15 * crt_cycles_multiplier);
						_outputPosition += 15;
						_crt->output_data(80 * crt_cycles_multiplier);
					}

					if(line_position >= 24 && line_position < 104)
					{
						// TODO: actually output some pixels, why not?
						_outputPosition++;
					}

					if(line_position == 104)
					{
						_crt->output_blank(24 * crt_cycles_multiplier);
						_outputPosition += 24;
					}
				}
			}
		}
	}
}
