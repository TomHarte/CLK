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

Machine::Machine()
{
	_crt = new Outputs::CRT(128, 312, 1, 1);
	_interruptStatus = 0x02;
	setup6502();
}

Machine::~Machine()
{
}

unsigned int Machine::perform_bus_operation(CPU6502::BusOperation operation, uint16_t address, uint8_t *value)
{
	if(address < 32768)
	{
		if(isReadOperation(operation))
		{
			*value = _ram[address];
		}
		else
		{
			_ram[address] = *value;
		}

		// TODO: RAM timing
	}
	else
	{
		if(address > 49152)
		{
			if((address & 0xff00) == 0xfe00)
			{
				printf("%c: %02x: ", isReadOperation(operation) ? 'r' : 'w', *value);

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
						}
						printf("Interrupt status or control\n");
					break;
					case 0x1:
					break;
					case 0x2:
						_screenStartAddress = (_screenStartAddress & 0xff00) | ((*value) & 0xe0);
						printf("Screen start address low, now %04x\n", _screenStartAddress);
					break;
					case 0x3:
						_screenStartAddress = (_screenStartAddress & 0x00ff) | (uint16_t)(((*value) & 0x3f) << 8);
						printf("Screen start address high, now %04x\n", _screenStartAddress);
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
							printf("Interrupt clear and paging\n");
						}
					break;
					case 0x6:
						printf("Counter\n");
					break;
					case 0x7:
						printf("Misc. control\n");
					break;
					default:
						printf("Palette\n");
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

	return 1;
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
