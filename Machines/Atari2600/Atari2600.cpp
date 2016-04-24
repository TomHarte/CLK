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
static const int horizontalTimerReload = 227;

Machine::Machine() :
	_timestamp(0),
	_horizontalTimer(0),
	_lastOutputStateDuration(0),
	_lastOutputState(OutputState::Sync),
	_piaTimerStatus(0xff),
	_rom(nullptr),
	_hMoveWillCount(false),
	_piaDataValue{0xff, 0xff},
	_tiaInputValue{0xff, 0xff}
{
	memset(_collisions, 0xff, sizeof(_collisions));
	set_reset_line(true);
}

void Machine::setup_output(float aspect_ratio)
{
	_crt = new Outputs::CRT::CRT(228, 1, Outputs::CRT::DisplayType::NTSC60, 2);
	_crt->set_composite_sampling_function(
		"float composite_sample(usampler2D texID, vec2 coordinate, vec2 iCoordinate, float phase, float amplitude)\n"
		"{\n"
			"return 0.9;"
//			"vec2 c = vec2(1.0);"//vec2(texture(texID, coordinate).rg) / vec2(255.0);"
//			"float y = 0.1 + c.x * 0.91071428571429;\n"
//			"float aOffset = 6.283185308 * (c.y - 3.0 / 16.0) * 1.14285714285714;\n"
//			"return y + step(0.03125, c.y) * 0.1 * cos((coordinate.x * 2.0 * 3.141592654) - aOffset);\n"
		"}");
	_crt->set_output_device(Outputs::CRT::Television);
}

Machine::~Machine()
{
	delete[] _rom;
	delete _crt;
}

void Machine::switch_region()
{
	_crt->set_new_display_type(228, Outputs::CRT::DisplayType::PAL50);
}

void Machine::get_output_pixel(uint8_t *pixel, int offset)
{
	// get the playfield pixel and hence a proposed colour
	uint8_t playfieldPixel = _playfield[offset >> 2];
	uint8_t playfieldColour = ((_playfieldControl&6) == 2) ? _playerColour[offset / 80] : _playfieldColour;

	// get player and missile proposed pixels
	uint8_t playerPixels[2] = {0, 0}, missilePixels[2] = {0, 0};
	for(int c = 0; c < 2; c++)
	{
		const uint8_t repeatMask = _playerAndMissileSize[c]&7;
		if(_playerGraphics[c])
		{
			// figure out player colour
			int flipMask = (_playerReflection[c]&0x8) ? 0 : 7;

			int relativeTimer = _objectCounter[c] - 5;
			switch (repeatMask)
			{
				case 0: break;
				default:
					if (repeatMask&4 && relativeTimer >= 64) relativeTimer -= 64;
					else if (repeatMask&2 && relativeTimer >= 32) relativeTimer -= 32;
					else if (repeatMask&1 && relativeTimer >= 16) relativeTimer -= 16;
				break;
				case 5:
					relativeTimer >>= 1;
				break;
				case 7:
					relativeTimer >>= 2;
				break;
			}

			if(relativeTimer >= 0 && relativeTimer < 8)
				playerPixels[c] = (_playerGraphics[c] >> (relativeTimer ^ flipMask)) &1;
		}

		// figure out missile colour
		if((_missileGraphicsEnable[c]&2) && !(_missileGraphicsReset[c]&2))
		{
			int missileIndex = _objectCounter[2+c] - 4;
			switch (repeatMask)
			{
				case 0: break;
				default:
					if (repeatMask&4 && missileIndex >= 64) missileIndex -= 64;
					else if (repeatMask&2 && missileIndex >= 32) missileIndex -= 32;
					else if (repeatMask&1 && missileIndex >= 16) missileIndex -= 16;
				break;
				case 5:
					missileIndex >>= 1;
				break;
				case 7:
					missileIndex >>= 2;
				break;
			}
			int missileSize = 1 << ((_playerAndMissileSize[c] >> 4)&3);
			missilePixels[c] = (missileIndex >= 0 && missileIndex < missileSize) ? 1 : 0;
		}
	}

	// get the ball proposed colour
	uint8_t ballPixel = 0;
	if(_ballGraphicsEnable&2)
	{
		int ballIndex = _objectCounter[4] - 4;
		int ballSize = 1 << ((_playfieldControl >> 4)&3);
		ballPixel = (ballIndex >= 0 && ballIndex < ballSize) ? 1 : 0;
	}

	// accumulate collisions
	if(playerPixels[0] | playerPixels[1])
	{
		_collisions[0] |= ((missilePixels[0] & playerPixels[1]) << 7)	| ((missilePixels[0] & playerPixels[0]) << 6);
		_collisions[1] |= ((missilePixels[1] & playerPixels[0]) << 7)	| ((missilePixels[1] & playerPixels[1]) << 6);

		_collisions[2] |= ((playfieldPixel & playerPixels[0]) << 7)		| ((ballPixel & playerPixels[0]) << 6);
		_collisions[3] |= ((playfieldPixel & playerPixels[1]) << 7)		| ((ballPixel & playerPixels[1]) << 6);

		_collisions[7] |= ((playerPixels[0] & playerPixels[1]) << 7);
	}

	if(playfieldPixel | ballPixel)
	{
		_collisions[4] |= ((playfieldPixel & missilePixels[0]) << 7)	| ((ballPixel & missilePixels[0]) << 6);
		_collisions[5] |= ((playfieldPixel & missilePixels[1]) << 7)	| ((ballPixel & missilePixels[1]) << 6);

		_collisions[6] |= ((playfieldPixel & ballPixel) << 7);
	}

	if(missilePixels[0] & missilePixels[1])
		_collisions[7] |= (1 << 6);

	// apply appropriate priority to pick a colour
	playfieldPixel |= ballPixel;
	uint8_t outputColour = playfieldPixel ? playfieldColour : _backgroundColour;

	if(!(_playfieldControl&0x04) || !playfieldPixel) {
		if (playerPixels[1] || missilePixels[1]) outputColour = _playerColour[1];
		if (playerPixels[0] || missilePixels[0]) outputColour = _playerColour[0];
	}

	// map that colour to separate Y and phase components
	pixel[0] = (outputColour << 4)&0xe0;
	pixel[1] = outputColour&0xf0;
}

// in imputing the knowledge that all we're dealing with is the rollover from 159 to 0,
// this is faster than the straightforward +1)%160 per profiling
#define increment_object_counter(c) _objectCounter[c] = (_objectCounter[c]+1)&~((158-_objectCounter[c]) >> 8)

void Machine::output_pixels(unsigned int count)
{
	const int32_t start_of_sync = 214;
	const int32_t end_of_sync = 198;

	while(count--)
	{
		OutputState state;

		// update hmove
		if (!(_horizontalTimer&3)) {

			if(_hMoveFlags) {
				const uint8_t counterValue = _hMoveCounter ^ 0x7;
				for(int c = 0; c < 5; c++) {
					if (counterValue == (_objectMotion[c] >> 4)) _hMoveFlags &= ~(1 << c);
					if (_hMoveFlags&(1 << c)) increment_object_counter(c);
				}
			}

			if(_hMoveIsCounting) {
				_hMoveIsCounting = !!_hMoveCounter;
				_hMoveCounter = (_hMoveCounter-1)&0xf;
			}
		}


		// blank is decoded as 68 counts; sync and colour burst as 16 counts

		// 4 blank
		// 4 sync
		// 9 'blank'; colour burst after 4
		// 40 pixels

		// it'll be about 43 cycles from start of hsync to start of visible frame, so...
		// guesses, until I can find information: 26 cycles blank, 16 sync, 40 blank, 160 pixels
		if (_horizontalTimer < (_vBlankExtend ? 152 : 160)) {
			if(_vBlankEnabled) {
				state = OutputState::Blank;
			} else {
				state = OutputState::Pixel;
			}
		}
		else if (_horizontalTimer < end_of_sync) state = OutputState::Blank;
		else if (_horizontalTimer < start_of_sync) state = OutputState::Sync;
		else state = OutputState::Blank;

		// logic: if vsync is enabled, output the opposite of the automatic hsync output
		if(_vSyncEnabled) {
			state = (state = OutputState::Sync) ? OutputState::Blank : OutputState::Sync;
		}

		_lastOutputStateDuration++;
		if(state != _lastOutputState)
		{
			switch(_lastOutputState)
			{
				case OutputState::Blank:	_crt->output_blank(_lastOutputStateDuration);	break;
				case OutputState::Sync:		_crt->output_sync(_lastOutputStateDuration);	break;
				case OutputState::Pixel:	_crt->output_data(_lastOutputStateDuration,	1);	break;
			}
			_lastOutputStateDuration = 0;
			_lastOutputState = state;

			if(state == OutputState::Pixel)
			{
				_outputBuffer = _crt->allocate_write_area(160);
			} else {
				_outputBuffer = nullptr;
			}
		}

		if(_horizontalTimer < (_vBlankExtend ? 152 : 160))
		{
			if(_outputBuffer)
				get_output_pixel(&_outputBuffer[_lastOutputStateDuration << 1], 159 - _horizontalTimer);

			// increment all graphics counters
			increment_object_counter(0);
			increment_object_counter(1);
			increment_object_counter(2);
			increment_object_counter(3);
			increment_object_counter(4);
		}

		// assumption here: signed shifts right; otherwise it's just
		// an attempt to avoid both the % operator and a conditional
		_horizontalTimer--;
		const int32_t sign_extension = _horizontalTimer >> 31;
		_horizontalTimer = (_horizontalTimer&~sign_extension) | (sign_extension&horizontalTimerReload);

		if(!_horizontalTimer)
			_vBlankExtend = false;

		_timestamp ++;
	}
}

unsigned int Machine::perform_bus_operation(CPU6502::BusOperation operation, uint16_t address, uint8_t *value)
{
	set_reset_line(false);

	uint8_t returnValue = 0xff;
	unsigned int cycles_run_for = 1;
	const int32_t ready_line_disable_time = 227;//horizontalTimerReload;

	if(operation == CPU6502::BusOperation::Ready) {
		unsigned int distance_to_end_of_ready = (_horizontalTimer - ready_line_disable_time + horizontalTimerReload + 1)%(horizontalTimerReload + 1);
		cycles_run_for = distance_to_end_of_ready / 3;
		output_pixels(distance_to_end_of_ready);
	} else {
		output_pixels(3);
	}

	if(_hMoveWillCount) {
		_hMoveCounter = 0x0f;
		_hMoveFlags = 0x1f;
		_hMoveIsCounting = true;
		_hMoveWillCount = false;
	}

	if(_horizontalTimer == ready_line_disable_time)
		set_ready_line(false);

	if(operation != CPU6502::BusOperation::Ready) {

		// check for a paging access
		if(_rom_size > 4096 && ((address & 0x1f00) == 0x1f00)) {
			uint8_t *base_ptr = _romPages[0];
			uint8_t first_paging_register = (uint8_t)(0xf8 - (_rom_size >> 14)*2);

			const uint8_t paging_register = address&0xff;
			if(paging_register >= first_paging_register) {
				const uint16_t selected_page = paging_register - first_paging_register;
				if(selected_page * 4096 < _rom_size) {
					base_ptr = &_rom[selected_page * 4096];
				}
			}

			if(base_ptr != _romPages[0]) {
				_romPages[0] = base_ptr;
				_romPages[1] = base_ptr + 1024;
				_romPages[2] = base_ptr + 2048;
				_romPages[3] = base_ptr + 3072;
			}
		}

		// check for a ROM read
		if ((address&0x1000) && isReadOperation(operation)) {
			returnValue &= _romPages[(address >> 10)&3][address&1023];
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
				const uint16_t decodedAddress = address & 0xf;
				switch(decodedAddress) {
					case 0x00:		// missile 0 / player collisions
					case 0x01:		// missile 1 / player collisions
					case 0x02:		// player 0 / playfield / ball collisions
					case 0x03:		// player 1 / playfield / ball collisions
					case 0x04:		// missile 0 / playfield / ball collisions
					case 0x05:		// missile 1 / playfield / ball collisions
					case 0x06:		// ball / playfield collisions
					case 0x07:		// player / player, missile / missile collisions
						returnValue &= _collisions[decodedAddress];
					break;

					case 0x08:
					case 0x09:
					case 0x0a:
					case 0x0b:
						// TODO: pot ports
					break;

					case 0x0c:
					case 0x0d:
						returnValue &= _tiaInputValue[decodedAddress - 0x0c];
					break;
				}
			} else {
				const uint16_t decodedAddress = address & 0x3f;
				switch(decodedAddress) {
					case 0x00:
						_vSyncEnabled = !!(*value & 0x02);
					break;
					case 0x01:	_vBlankEnabled = !!(*value & 0x02);	break;

					case 0x02:
						set_ready_line(true);
					break;
					case 0x03:
						_horizontalTimer = 0;
					break;

					case 0x04:
					case 0x05: _playerAndMissileSize[decodedAddress - 0x04] = *value;	break;

					case 0x06:
					case 0x07: _playerColour[decodedAddress - 0x06] = *value;	break;
					case 0x08: _playfieldColour = *value;	break;
					case 0x09: _backgroundColour = *value;	break;

					case 0x0a: {
						uint8_t old_playfield_control = _playfieldControl;
						_playfieldControl = *value;

						// did the mirroring bit change?
						if((_playfieldControl^old_playfield_control)&1) {
							if(_playfieldControl&1) {
								for(int c = 0; c < 20; c++) _playfield[c+20] = _playfield[19-c];
							} else {
								memcpy(&_playfield[20], _playfield, 20);
							}
						}
					} break;
					case 0x0b:
					case 0x0c: _playerReflection[decodedAddress - 0x0b] = *value;	break;

					case 0x0d:
						_playfield[0] = ((*value) >> 4)&1;
						_playfield[1] = ((*value) >> 5)&1;
						_playfield[2] = ((*value) >> 6)&1;
						_playfield[3] = (*value) >> 7;

						if(_playfieldControl&1) {
							for(int c = 0; c < 4; c++) _playfield[39-c] = _playfield[c];
						} else {
							memcpy(&_playfield[20], _playfield, 4);
						}
					break;
					case 0x0e:
						_playfield[4] = (*value) >> 7;
						_playfield[5] = ((*value) >> 6)&1;
						_playfield[6] = ((*value) >> 5)&1;
						_playfield[7] = ((*value) >> 4)&1;
						_playfield[8] = ((*value) >> 3)&1;
						_playfield[9] = ((*value) >> 2)&1;
						_playfield[10] = ((*value) >> 1)&1;
						_playfield[11] = (*value)&1;

						if(_playfieldControl&1) {
							for(int c = 0; c < 8; c++) _playfield[35-c] = _playfield[c+4];
						} else {
							memcpy(&_playfield[24], &_playfield[4], 8);
						}
					break;
					case 0x0f:
						_playfield[19] = (*value) >> 7;
						_playfield[18] = ((*value) >> 6)&1;
						_playfield[17] = ((*value) >> 5)&1;
						_playfield[16] = ((*value) >> 4)&1;
						_playfield[15] = ((*value) >> 3)&1;
						_playfield[14] = ((*value) >> 2)&1;
						_playfield[13] = ((*value) >> 1)&1;
						_playfield[12] = (*value)&1;

						if(_playfieldControl&1) {
							for(int c = 0; c < 8; c++) _playfield[27-c] = _playfield[c+12];
						} else {
							memcpy(&_playfield[32], &_playfield[12], 8);
						}
					break;

					case 0x10:	case 0x11:	case 0x12:	case 0x13:
					case 0x14: _objectCounter[decodedAddress - 0x10] = 0;		break;

					case 0x1c:
						_ballGraphicsEnable = _ballGraphicsEnableLatch;
					case 0x1b: {
						int index = decodedAddress - 0x1b;
						_playerGraphicsLatch[index] = *value;
						if(!(_playerGraphicsLatchEnable[index]&1))
							_playerGraphics[index] = _playerGraphicsLatch[index];
						_playerGraphics[index^1] = _playerGraphicsLatch[index^1];
					} break;
					case 0x1d: _missileGraphicsEnable[0] = *value;	break;
					case 0x1e: _missileGraphicsEnable[1] = *value;	break;
					case 0x1f:
						_ballGraphicsEnableLatch = *value;
						if(!(_ballGraphicsEnableDelay&1))
							_ballGraphicsEnable = _ballGraphicsEnableLatch;
					break;

					case 0x20:
					case 0x21:
					case 0x22:
					case 0x23:
					case 0x24:
						_objectMotion[decodedAddress - 0x20] = *value;
					break;

					case 0x25: _playerGraphicsLatchEnable[0] = *value;	break;
					case 0x26: _playerGraphicsLatchEnable[1] = *value;	break;
					case 0x27: _ballGraphicsEnableDelay = *value;		break;

					case 0x28:
					case 0x29:
						if (!(*value&0x02) && _missileGraphicsReset[decodedAddress - 0x28]&0x02)
							_objectCounter[decodedAddress - 0x26] = _objectCounter[decodedAddress - 0x28];  // TODO: +3 for normal, +6 for double, +10 for quad
						_missileGraphicsReset[decodedAddress - 0x28] = *value;
					break;

					case 0x2a:
						_vBlankExtend = true;
						_hMoveWillCount = true;
					break;
					case 0x2b:
						_objectMotion[0] =
						_objectMotion[1] =
						_objectMotion[2] =
						_objectMotion[3] =
						_objectMotion[4] = 0;
					break;
					case 0x2c:
						_collisions[0] = _collisions[1] = _collisions[2] = 
						_collisions[3] = _collisions[4] = _collisions[5] = 0x3f;
						_collisions[6] = 0x7f;
						_collisions[7] = 0x3f;
					break;
				}
			}
		}

		// check for a PIA access
		if ((address&0x1280) == 0x280) {
			if(isReadOperation(operation)) {
				const uint8_t decodedAddress = address & 0xf;
				switch(address & 0xf) {
					case 0x00:
					case 0x02:
						returnValue &= _piaDataValue[decodedAddress / 2];
					break;
					case 0x01:
					case 0x03:
						// TODO: port DDR
					break;
					case 0x04:
						returnValue &= _piaTimerValue >> _piaTimerShift;

						if(_writtenPiaTimerShift != _piaTimerShift) {
							_piaTimerShift = _writtenPiaTimerShift;
							_piaTimerValue <<= _writtenPiaTimerShift;
						}
					break;
					case 0x05:
						returnValue &= _piaTimerStatus;
						_piaTimerStatus &= ~0x40;
					break;
				}
			} else {
				const uint8_t decodedAddress = address & 0x0f;
				switch(decodedAddress) {
					case 0x04:
					case 0x05:
					case 0x06:
					case 0x07:
						_writtenPiaTimerShift = _piaTimerShift = (decodedAddress - 0x04) * 3 + (decodedAddress / 0x07);
						_piaTimerValue = (unsigned int)(*value << _piaTimerShift);
						_piaTimerStatus &= ~0xc0;
					break;
				}
			}
		}

		if(isReadOperation(operation)) {
			*value = returnValue;
		}
	}

	if(_piaTimerValue >= cycles_run_for) {
		_piaTimerValue -= cycles_run_for;
	} else {
		_piaTimerValue += 0xff - cycles_run_for;
		_piaTimerShift = 0;
		_piaTimerStatus |= 0xc0;
	}

	return cycles_run_for;
}

void Machine::set_digital_input(Atari2600DigitalInput input, bool state)
{
	switch (input) {
		case Atari2600DigitalInputJoy1Up:		if(state) _piaDataValue[0] &= ~0x10; else _piaDataValue[0] |= 0x10; break;
		case Atari2600DigitalInputJoy1Down:		if(state) _piaDataValue[0] &= ~0x20; else _piaDataValue[0] |= 0x20; break;
		case Atari2600DigitalInputJoy1Left:		if(state) _piaDataValue[0] &= ~0x40; else _piaDataValue[0] |= 0x40; break;
		case Atari2600DigitalInputJoy1Right:	if(state) _piaDataValue[0] &= ~0x80; else _piaDataValue[0] |= 0x80; break;

		case Atari2600DigitalInputJoy2Up:		if(state) _piaDataValue[0] &= ~0x01; else _piaDataValue[0] |= 0x01; break;
		case Atari2600DigitalInputJoy2Down:		if(state) _piaDataValue[0] &= ~0x02; else _piaDataValue[0] |= 0x02; break;
		case Atari2600DigitalInputJoy2Left:		if(state) _piaDataValue[0] &= ~0x04; else _piaDataValue[0] |= 0x04; break;
		case Atari2600DigitalInputJoy2Right:	if(state) _piaDataValue[0] &= ~0x08; else _piaDataValue[0] |= 0x08; break;

		// TODO: latching
		case Atari2600DigitalInputJoy1Fire:		if(state) _tiaInputValue[0] &= ~0x80; else _tiaInputValue[0] |= 0x80; break;
		case Atari2600DigitalInputJoy2Fire:		if(state) _tiaInputValue[1] &= ~0x80; else _tiaInputValue[1] |= 0x80; break;

		default: break;
	}
}

void Machine::set_rom(size_t length, const uint8_t *data)
{
	_rom_size = 1024;
	while(_rom_size < length && _rom_size < 32768) _rom_size <<= 1;

	delete[] _rom;

	_rom = new uint8_t[_rom_size];

	size_t offset = 0;
	const size_t copy_step = std::min(_rom_size, length);
	while(offset < _rom_size)
	{
		size_t copy_length = std::min(copy_step, _rom_size - offset);
		memcpy(&_rom[offset], data, copy_length);
		offset += copy_length;
	}

	size_t romMask = _rom_size - 1;
	_romPages[0] = _rom;
	_romPages[1] = &_rom[1024 & romMask];
	_romPages[2] = &_rom[2048 & romMask];
	_romPages[3] = &_rom[3072 & romMask];
}
