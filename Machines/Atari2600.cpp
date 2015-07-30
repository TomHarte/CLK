//
//  Atari2600.cpp
//  CLK
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
	_horizontalTimer = 227;
	_lastOutputStateDuration = 0;
	_lastOutputState = OutputState::Sync;
	_crt = new Outputs::CRT(228, 256, 1, 4);

	reset();
}

Machine::~Machine()
{
	delete _crt;
}

void Machine::get_output_pixel(uint8_t *pixel, int offset)
{
	// get the playfield pixel
	const int x = offset >> 2;
	const int mirrored = (x / 20) & (_playfieldControl&1);
	const int index = mirrored ? x - 20 : 19 - (x%20);
	const int byte = 2 - (index >> 3);
	const int lowestBit = (byte&1)^1;
	const int bit = (index & 7)^(lowestBit | (lowestBit << 1) | (lowestBit << 2));

	uint8_t playFieldPixel = (_playfield[byte] >> bit)&1;

	const uint8_t palette[16][3] =
	{
		{255, 255, 255},	{253, 250, 115},	{236, 199, 125},	{252, 187, 151},
		{252, 180, 181},	{235, 177, 223},	{211, 178, 250},	{187, 182, 250},
		{164, 186, 250},	{166, 201, 250},	{164, 224, 251},	{165, 251, 213},
		{185, 251, 187},	{201, 250, 168},	{225, 235, 160},	{252, 223, 145}
	};
	const uint8_t alphaValues[8] =
	{
//		0, 64, 108, 144, 176, 200, 220, 255
//	};
//
//	{
		69, 134, 108, 161, 186, 210, 235, 255
	};

	// TODO: almost everything!
	uint8_t playfieldColour = ((_playfieldControl&6) == 2) ? ((x < 20) ? _player0Colour : _player1Colour) : _playfieldColour;

	uint8_t outputColour = playFieldPixel ? playfieldColour : _backgroundColour;

	if(_horizontalTimer == _playerPosition[0]) outputColour = _player0Colour;
	if(_horizontalTimer == _playerPosition[1]) outputColour = _player1Colour;

	pixel[0] = palette[outputColour >> 4][0];
	pixel[1] = palette[outputColour >> 4][1];
	pixel[2] = palette[outputColour >> 4][2];
	pixel[3] = alphaValues[(outputColour >> 1)&7];
}

void Machine::output_pixels(int count)
{
	while(count--)
	{
		OutputState state;

		// logic: if in vsync, output that; otherwise if in vblank then output that;
		// otherwise output a pixel
		if(_vSyncEnabled) {
			state = OutputState::Sync;
		} else {

			// blank is decoded as 68 counts; sync and colour burst as 16 counts

			// it'll be about 43 cycles from start of hsync to start of visible frame, so...
			// guesses, until I can find information: 26 cycles blank, 16 sync, 40 blank, 160 pixels
			if(_horizontalTimer > 214) state = OutputState::Blank;
			else if (_horizontalTimer > 188) state = OutputState::Sync;
			else if (_horizontalTimer >= 160) state = OutputState::Blank;
			else {
				if(_vBlankEnabled) {
					state = OutputState::Blank;
				} else {
					state = OutputState::Pixel;
				}
			}
		}

		_lastOutputStateDuration++;
		if(state != _lastOutputState)
		{
			switch(_lastOutputState)
			{
				case OutputState::Blank:	_crt->output_blank(_lastOutputStateDuration);					break;
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

		if(state == OutputState::Pixel && _outputBuffer)
			get_output_pixel(&_outputBuffer[_lastOutputStateDuration * 4], 159 - _horizontalTimer);

		// assumption here: signed shifts right; otherwise it's just
		// an attempt to avoid both the % operator and a conditional
		_horizontalTimer--;
		const int32_t sign_extension = _horizontalTimer >> 31;
		_horizontalTimer = (_horizontalTimer&~sign_extension) | (sign_extension&227);
	}
}

int Machine::perform_bus_operation(CPU6502::BusOperation operation, uint16_t address, uint8_t *value)
{
	uint8_t returnValue = 0xff;
	int cycle_count = 1;

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
			switch(address & 0xf) {
				case 0x00:	*value = 0x3f;	break;
				case 0x01:	*value = 0x3f;	break;
				case 0x02:	*value = 0x3f;	break;
				case 0x03:	*value = 0x3f;	break;
				case 0x04:	*value = 0x3f;	break;
				case 0x05:	*value = 0x3f;	break;
				case 0x06:	*value = 0x7f;	break;
				case 0x07:	*value = 0x3f;	break;
			}
		} else {
			switch(address & 0x3f) {
				case 0x00:	_vSyncEnabled = !!(*value & 0x02);	break;
				case 0x01:	_vBlankEnabled = !!(*value & 0x02);	break;

				case 0x02: {
					cycle_count = _horizontalTimer / 3;
					output_pixels(3 * cycle_count);
				} break;
				case 0x03: _horizontalTimer = 227; break;

				case 0x04: _playerAndMissileSize[0] = *value;	break;
				case 0x05: _playerAndMissileSize[1] = *value;	break;

				case 0x06: _player0Colour = *value;		break;
				case 0x07: _player1Colour = *value;		break;
				case 0x08: _playfieldColour = *value;	break;
				case 0x09: _backgroundColour = *value;	break;

				case 0x0a: _playfieldControl = *value;		break;
				case 0x0b: _playerReflection[0] = *value;	break;
				case 0x0c: _playerReflection[1] = *value;	break;
				case 0x0d: _playfield[0] = *value;			break;
				case 0x0e: _playfield[1] = *value;			break;
				case 0x0f: _playfield[2] = *value;			break;

				case 0x10: _playerPosition[0] = _horizontalTimer;	break;
				case 0x11: _playerPosition[1] = _horizontalTimer;	break;
				case 0x12: _missilePosition[0] = _horizontalTimer;	break;
				case 0x13: _missilePosition[1] = _horizontalTimer;	break;
				case 0x14: _ballPosition = _horizontalTimer;		break;

				case 0x1b: _playerGraphics[0] = *value;			break;
				case 0x1c: _playerGraphics[1] = *value;			break;
				case 0x1d: _missileGraphicsEnable[0] = *value;	break;
				case 0x1e: _missileGraphicsEnable[1] = *value;	break;
				case 0x1f: _ballGraphicsEnable = *value;		break;
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

	_piaTimerValue -= cycle_count;

	return cycle_count;
}

void Machine::set_rom(size_t length, const uint8_t *data)
{
	length = std::min((size_t)4096, length);
	memcpy(_rom, data, length);
	_romMask = length - 1;
	reset();
}
