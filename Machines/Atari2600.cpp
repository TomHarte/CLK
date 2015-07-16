//
//  Atari2600.cpp
//  ElectrEm
//
//  Created by Thomas Harte on 14/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

#include "Atari2600.hpp"
#include <algorithm>
#include <stdio.h>

using namespace Atari2600;

Machine::Machine()
{
	reset();
	_timestamp = 0;
	_horizontalTimer = 0;
	_pixelPosition = 0;
}

void Machine::output_pixels(int count)
{
	while(count--) {
		int x = _pixelPosition >> 2;
		bool mirrored = (x / 20) && !!(_playFieldControl&1);
		int index = mirrored ? (x%20) : 39 - x;
		int byte = 2 - (index >> 3);
		int bit = (index & 7)^((byte&1) ? 0 : 7);

		_playFieldPixel = (_playField[byte] >> bit)&1;

//		if(!(_pixelPosition&3))
//			fputc(_playFieldPixel && !_vblank ? '*' : ' ', stdout);

		_pixelPosition++;
	}
}

void Machine::perform_bus_operation(CPU6502::BusOperation operation, uint16_t address, uint8_t *value)
{
	static int lines = 0;
	uint8_t returnValue = 0xff;

	_timestamp++;

	if (_horizontalTimer == 228) {
		_horizontalTimer = 0;
		_pixelPosition = 0;
//		printf("\n");
		lines++;
	}

	if (_horizontalTimer == 69) {
		output_pixels(1);
	}

	if (_horizontalTimer >= 70) {
		output_pixels(3);
	}

	_horizontalTimer += 3;


	// check for a ROM access
	if ((address&0x1000) && isReadOperation(operation)) {
//		if(operation == CPU6502::BusOperation::ReadOpcode) printf("[%04x]\n", address);
		returnValue &= _rom[address&_romMask];
	}

	// check for a RAM access
	if ((address&0x1280) == 0x80) {

		if(isReadOperation(operation)) {
			returnValue &= _ram[address&0x7f];
		} else {
			_ram[address&0x7f] = *value;
		}
	}

	// check for a TIA access
	if (!(address&0x1080)) {
		if(isReadOperation(operation)) {
		} else {
			switch(address & 0x3f) {
				case 0: {
					bool newVsync = !!(*value & 0x02);

					if (newVsync != _vsync) {
						_vsync = newVsync;
					}
				} break;

				case 1: {
					bool newVblank = !!(*value & 0x02);

					if (newVblank != _vblank) {
						_vblank = newVblank;
					}
				} break;

				case 2: {

					int pixelsToRun = 228 - _horizontalTimer;
					_piaTimerValue -= pixelsToRun;
					if (pixelsToRun > 160) pixelsToRun = 160;
					output_pixels(pixelsToRun);
					_horizontalTimer = 228;
				} break;
				case 3: _horizontalTimer = 0; break;

				case 0x0a: _playFieldControl = *value; break;
				case 0x0d: _playField[0] = *value; break;
				case 0x0e: _playField[1] = *value; break;
				case 0x0f: _playField[2] = *value; break;
			}
		}
//		printf("Uncaught TIA %04x\n", address);
	}

	// check for a PIA access
	if ((address&0x1280) == 0x280) {
		if(isReadOperation(operation)) {
			switch(address & 0xf) {
				case 0x04: returnValue &= _piaTimerValue >> _piaTimerShift; break;
			}
		} else {
			switch(address & 0x0f) {
				case 0x04:	_piaTimerShift = 0;		_piaTimerValue = *value << 0; break;
				case 0x05:	_piaTimerShift = 3;		_piaTimerValue = *value << 3; break;
				case 0x06:	_piaTimerShift = 6;		_piaTimerValue = *value << 6; break;
				case 0x07:	_piaTimerShift = 10;	_piaTimerValue = *value << 10; break;
			}
		}
//		printf("Uncaught PIA %04x\n", address);
	}

	if(isReadOperation(operation)) {
		*value = returnValue;
	}

	_piaTimerValue--;
}

void Machine::set_rom(size_t length, const uint8_t *data)
{
	length = std::min((size_t)4096, length);
	memcpy(_rom, data, length);
	_romMask = length - 1;
	reset();
}
