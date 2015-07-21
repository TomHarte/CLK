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
static const char atari2600DataType[] = "Atari2600";

Machine::Machine()
{
	_timestamp = 0;
	_horizontalTimer = 0;
	_lastOutputStateDuration = 0;
	_lastOutputState = OutputState::Sync;
	_crt = new Outputs::CRT(228, 256, 1, 4);

	reset();
}

void Machine::get_output_pixel(uint8_t *pixel, int offset)
{
	// get the playfield pixel
	const int x = offset >> 2;
	const int mirrored = (x / 20) & (_playFieldControl&1);
	const int index = mirrored ? x - 20 : 19 - (x%20);
	const int byte = 2 - (index >> 3);
	const int lowestBit = (byte&1)^1;
	const int bit = (index & 7)^(lowestBit | (lowestBit << 1) | (lowestBit << 2));

	uint8_t playFieldPixel = (_playField[byte] >> bit)&1;

	// TODO: almost everything!
	pixel[0] = playFieldPixel ? 0xff : 0x00;
	pixel[1] = playFieldPixel ? 0xff : 0x00;
	pixel[2] = playFieldPixel ? 0xff : 0x00;
}

void Machine::output_pixels(int count)
{
	while(count--)
	{
		// logic: if in vsync, output that; otherwise if in vblank then output that;
		// otherwise output a pixel
		if(_vSyncEnabled) {
			output_state(OutputState::Sync, nullptr);
		} else {

			// blank is decoded as 68 counts; sync and colour burst as 16 counts

			// guesses, until I can find information: 26 cycles blank, 16 sync, 26 blank, 160 pixels
			if(_horizontalTimer < 26) output_state(OutputState::Blank, nullptr);
			else if (_horizontalTimer < 42) output_state(OutputState::Sync, nullptr);
			else if (_horizontalTimer < 68) output_state(OutputState::Blank, nullptr);
			else {
				if(_vBlankEnabled) {
					output_state(OutputState::Blank, nullptr);
				} else {
					uint8_t outputPixel[3];
					get_output_pixel(outputPixel, _horizontalTimer - 68);
					output_state(OutputState::Pixel, outputPixel);
				}
			}
		}

		_horizontalTimer = (_horizontalTimer + 1)%228;
	}
}

void Machine::output_state(OutputState state, uint8_t *pixel)
{
	_lastOutputStateDuration++;
	if(state != _lastOutputState)
	{
		switch(_lastOutputState)
		{
			case OutputState::Blank:	{
				_crt->allocate_write_area(1);
				_outputBuffer = _crt->get_write_target_for_buffer(0);
				_outputBuffer[0] = _outputBuffer[1] = _outputBuffer[2] = 0;
				_outputBuffer[3] = 0xff;
				_crt->output_level(_lastOutputStateDuration, atari2600DataType);
			} break;
			case OutputState::Sync:		_crt->output_sync(_lastOutputStateDuration);					break;
			case OutputState::Pixel:	_crt->output_data(_lastOutputStateDuration, atari2600DataType);	break;
		}
		_lastOutputStateDuration = 0;
		_lastOutputState = state;

		if(state == OutputState::Pixel)
		{
			_crt->allocate_write_area(160);
			_outputBuffer = _crt->get_write_target_for_buffer(0);
		}
	}

	if(state == OutputState::Pixel)
	{
		_outputBuffer[(_lastOutputStateDuration * 4) + 0] = pixel[0];
		_outputBuffer[(_lastOutputStateDuration * 4) + 1] = pixel[1];
		_outputBuffer[(_lastOutputStateDuration * 4) + 2] = pixel[2];
		_outputBuffer[(_lastOutputStateDuration * 4) + 3] = 0xff;
	}
}


void Machine::perform_bus_operation(CPU6502::BusOperation operation, uint16_t address, uint8_t *value)
{
	uint8_t returnValue = 0xff;

	output_pixels(3);

	_timestamp++;

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
				case 0:	_vSyncEnabled = !!(*value & 0x02);	break;
				case 1:	_vBlankEnabled = !!(*value & 0x02);	break;

				case 2: {
					const int cyclesToRunFor = 228 - _horizontalTimer;
					_piaTimerValue -= cyclesToRunFor;
					output_pixels(cyclesToRunFor);
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
