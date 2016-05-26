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
namespace {
	static const unsigned int horizontalTimerPeriod = 228;
}

Machine::Machine() :
	_horizontalTimer(0),
	_lastOutputStateDuration(0),
	_lastOutputState(OutputState::Sync),
	_piaTimerStatus(0xff),
	_rom(nullptr),
	_piaDataValue{0xff, 0xff},
	_tiaInputValue{0xff, 0xff},
	_upcomingEventsPointer(0)
{
	memset(_collisions, 0xff, sizeof(_collisions));
	set_reset_line(true);
}

void Machine::setup_output(float aspect_ratio)
{
	_crt = new Outputs::CRT::CRT(228, 1, 263, Outputs::CRT::ColourSpace::YIQ, 228, 1, 1);

	// this is the NTSC phase offset function; see below for PAL
	_crt->set_composite_sampling_function(
		"float composite_sample(usampler2D texID, vec2 coordinate, vec2 iCoordinate, float phase, float amplitude)"
		"{"
			"uint c = texture(texID, coordinate).r;"
			"uint y = c & 14u;"
			"uint iPhase = (c >> 4);"

			"float phaseOffset = 6.283185308 * float(iPhase - 1u) / 13.0;"
			"return (float(y) / 14.0) * (1.0 - amplitude) + step(1, iPhase) * amplitude * cos(phase + phaseOffset);"
		"}");
	_crt->set_output_device(Outputs::CRT::Television);
}

void Machine::switch_region()
{
	// the PAL function
	_crt->set_composite_sampling_function(
		"float composite_sample(usampler2D texID, vec2 coordinate, vec2 iCoordinate, float phase, float amplitude)"
		"{"
			"uint c = texture(texID, coordinate).r;"
			"uint y = c & 14u;"
			"uint iPhase = (c >> 4);"

			"uint direction = iPhase & 1u;"
			"float phaseOffset = float(7u - direction) + (float(direction) - 0.5) * 2.0 * float(iPhase >> 1);"
			"phaseOffset *= 6.283185308 / 12.0;"
			"return (float(y) / 14.0) * (1.0 - amplitude) + step(4, (iPhase + 2u) & 15u) * amplitude * cos(phase + phaseOffset);"
		"}");
	_crt->set_new_timing(228, 312, Outputs::CRT::ColourSpace::YUV, 228, 1);
}

void Machine::close_output()
{
	delete _crt;
	_crt = nullptr;
}

Machine::~Machine()
{
	delete[] _rom;
	close_output();
}

void Machine::update_timers(int mask)
{
	unsigned int upcomingPointerPlus4 = (_upcomingEventsPointer + 4)%number_of_upcoming_events;
	unsigned int upcomingPointerPlus5 = (_upcomingEventsPointer + 5)%number_of_upcoming_events;
	unsigned int upcomingPointerPlus6 = (_upcomingEventsPointer + 6)%number_of_upcoming_events;

	// grab the background now, for application in four clocks
	if(mask & (1 << 5) && !(_horizontalTimer&3))
	{
		unsigned int offset = 4 + _horizontalTimer - (horizontalTimerPeriod - 160);
		_upcomingEvents[upcomingPointerPlus4].updates |= Event::Action::Playfield;
		_upcomingEvents[upcomingPointerPlus4].playfieldPixel = _playfield[(offset >> 2)%40];
	}

	if(mask & (1 << 4))
	{
		// the ball becomes visible whenever it hits zero, regardless of whether its status
		// is the result of a counter rollover or a programmatic reset
		if(!_objectCounter[4].count) _upcomingEvents[upcomingPointerPlus4].pixelCounterResetMask &= ~(1 << 4);
	}

	// check for player and missle triggers
	for(int c = 0; c < 4; c++)
	{
		if(mask & (1 << c))
		{
			// the players and missles become visible only upon overflow to zero, so schedule for
			// 1/2 clocks ahead from 159
			if(_objectCounter[c].count == 159)
			{
				unsigned int actionSlot = (c < 2) ? upcomingPointerPlus6 : upcomingPointerPlus5;
				_upcomingEvents[actionSlot].pixelCounterResetMask &= ~(1 << c);
			}
			else
			{
				// otherwise visibility is determined by an appropriate repeat mask and hitting any of 12, 28 or 60,
				// in which case the counter reset (and hence the start of drawing) will occur in 4/5 cycles
				uint8_t repeatMask = _playerAndMissileSize[c&1] & 7;
				if(
					( _objectCounter[c].count == 16 && ((repeatMask == 1) || (repeatMask == 3)) ) ||
					( _objectCounter[c].count == 32 && ((repeatMask == 2) || (repeatMask == 3) || (repeatMask == 6)) ) ||
					( _objectCounter[c].count == 64 && ((repeatMask == 4) || (repeatMask == 6)) )
				)
				{
					unsigned int actionSlot = (c < 2) ? upcomingPointerPlus5 : upcomingPointerPlus4;
					_upcomingEvents[actionSlot].pixelCounterResetMask &= ~(1 << c);
				}
			}
		}
	}

	// update the pixel counters
	for(int c = 0; c < 2; c++)
	{
		if(mask&(1 << c))
		{
			_objectCounter[c].broad_pixel++;

			uint8_t repeatMask = _playerAndMissileSize[c] & 7;
			switch(repeatMask)
			{
				default:	_objectCounter[c].pixel ++;	break;
				case 5:		_objectCounter[c].pixel += _objectCounter[c].broad_pixel&1;	break;
				case 7:		_objectCounter[c].pixel += ((_objectCounter[c].broad_pixel | (_objectCounter[c].broad_pixel >> 1))^1)&1;	break;
			}
			_objectCounter[c].count = (_objectCounter[c].count + 1)%160;
		}
	}

	for(int c = 2; c < 5; c++)
	{
		if(mask&(1 << c))
		{
			_objectCounter[c].count = (_objectCounter[c].count + 1)%160;
			_objectCounter[c].pixel ++;
		}
	}
}

uint8_t Machine::get_output_pixel()
{
	unsigned int offset = _horizontalTimer - (horizontalTimerPeriod - 160);

	// get the playfield pixel and hence a proposed colour
	uint8_t playfieldColour = ((_playfieldControl&6) == 2) ? _playerColour[offset / 80] : _playfieldColour;

	uint8_t ballPixel = 0;
	if(_objectCounter[4].pixel < 8 && _ballGraphicsEnable[_ballGraphicsSelector]&2) {
		int ballSize = 1 << ((_playfieldControl >> 4)&3);
		ballPixel = (_objectCounter[4].pixel < ballSize) ? 1 : 0;
	}

	// determine the pixel masks
	uint8_t playerPixels[2] = { 0, 0 };
	uint8_t missilePixels[2] = { 0, 0 };
	for(int c = 0; c < 2; c++)
	{
		if(_playerGraphics[c] && _objectCounter[c].pixel < 8) {
			int flipMask = (_playerReflection[c]&0x8) ? 0 : 7;
			playerPixels[c] = (_playerGraphics[_playerGraphicsSelector[c]][c] >> (_objectCounter[c].pixel ^ flipMask)) & 1;
		}

		if(_objectCounter[c+2].pixel < 8 && (_missileGraphicsEnable[c]&2) && !_missileGraphicsReset[c]) {
			int missileSize = 1 << ((_playerAndMissileSize[c] >> 4)&3);
			missilePixels[c] = (_objectCounter[c+2].pixel < missileSize) ? 1 : 0;
		}
	}

	// accumulate collisions
	if(playerPixels[0] | playerPixels[1]) {
		_collisions[0] |= ((missilePixels[0] & playerPixels[1]) << 7)	| ((missilePixels[0] & playerPixels[0]) << 6);
		_collisions[1] |= ((missilePixels[1] & playerPixels[0]) << 7)	| ((missilePixels[1] & playerPixels[1]) << 6);

		_collisions[2] |= ((_playfieldOutput & playerPixels[0]) << 7)	| ((ballPixel & playerPixels[0]) << 6);
		_collisions[3] |= ((_playfieldOutput & playerPixels[1]) << 7)	| ((ballPixel & playerPixels[1]) << 6);

		_collisions[7] |= ((playerPixels[0] & playerPixels[1]) << 7);
	}

	if(_playfieldOutput | ballPixel) {
		_collisions[4] |= ((_playfieldOutput & missilePixels[0]) << 7)	| ((ballPixel & missilePixels[0]) << 6);
		_collisions[5] |= ((_playfieldOutput & missilePixels[1]) << 7)	| ((ballPixel & missilePixels[1]) << 6);

		_collisions[6] |= ((_playfieldOutput & ballPixel) << 7);
	}

	if(missilePixels[0] & missilePixels[1])
		_collisions[7] |= (1 << 6);

	// apply appropriate priority to pick a colour
	uint8_t playfieldPixel = _playfieldOutput | ballPixel;
	uint8_t outputColour = playfieldPixel ? playfieldColour : _backgroundColour;

	if(!(_playfieldControl&0x04) || !playfieldPixel) {
		if(playerPixels[1] || missilePixels[1]) outputColour = _playerColour[1];
		if(playerPixels[0] || missilePixels[0]) outputColour = _playerColour[0];
	}

	// return colour
	return outputColour;
}

// in imputing the knowledge that all we're dealing with is the rollover from 159 to 0,
// this is faster than the straightforward +1)%160 per profiling
//#define increment_object_counter(c) _objectCounter[c] = (_objectCounter[c]+1)&~((158-_objectCounter[c]) >> 8)

void Machine::output_pixels(unsigned int count)
{
	while(count--)
	{
		OutputState state;

		// determine which output state will be active in four cycles from now
		switch(_horizontalTimer >> 2)
		{
			case 56: case 0: case 1: case 2:				state = OutputState::Blank;											break;
			case 3: case 4: case 5: case 6:					state = OutputState::Sync;											break;
			case 7: case 8: case 9: case 10:				state = OutputState::ColourBurst;									break;
			case 11: case 12: case 13:
			case 14: case 15:								state = OutputState::Blank;											break;

			case 16: case 17:								state = _vBlankExtend ? OutputState::Blank : OutputState::Pixel;	break;
			default:										state = OutputState::Pixel;											break;
		}

		// update pixel timers
		if(state == OutputState::Pixel)
		{
			update_timers(~0);
			_upcomingEvents[(_upcomingEventsPointer+4)%number_of_upcoming_events].updates |= Event::Action::ClockPixels;
		}

		// if vsync is enabled, output the opposite of the automatic hsync output
		if(_vSyncEnabled) {
			state = (state = OutputState::Sync) ? OutputState::Blank : OutputState::Sync;
		}

		// write that state as the one that will become effective in four clocks
		_upcomingEvents[(_upcomingEventsPointer+4)%number_of_upcoming_events].state = state;

		// apply any queued changes and flush the record
		if(_upcomingEvents[_upcomingEventsPointer].updates & Event::Action::HMoveSetup)
		{
			_upcomingEvents[_upcomingEventsPointer].updates |= Event::Action::HMoveCompare;
			_vBlankExtend = true;

			// clear any ongoing moves
			if(_hMoveFlags)
			{
				for(int c = 0; c < number_of_upcoming_events; c++)
				{
					_upcomingEvents[c].updates &= ~(Event::Action::HMoveCompare | Event::Action::HMoveDecrement);
				}
			}

			// schedule new moves
			_hMoveFlags = 0x1f;
			_hMoveCounter = 15;
		}

		if(_upcomingEvents[_upcomingEventsPointer].updates & Event::Action::HMoveCompare)
		{
			for(int c = 0; c < 5; c++)
			{
				if(((_objectCounter[c].motion >> 4)^_hMoveCounter) == 7)
				{
					_hMoveFlags &= ~(1 << c);
				}
			}
			if(_hMoveFlags)
			{
				if(_hMoveCounter) _hMoveCounter--;
				_upcomingEvents[(_upcomingEventsPointer+4)%number_of_upcoming_events].updates |= Event::Action::HMoveCompare;
				_upcomingEvents[(_upcomingEventsPointer+2)%number_of_upcoming_events].updates |= Event::Action::HMoveDecrement;
			}
		}

		if(_upcomingEvents[_upcomingEventsPointer].updates & Event::Action::HMoveDecrement)
		{
			update_timers(_hMoveFlags);
		}

		if(_upcomingEvents[_upcomingEventsPointer].updates & Event::Action::ClockPixels)
		{
		}

		// apply any resets
		_objectCounter[0].pixel *= (_upcomingEvents[_upcomingEventsPointer].pixelCounterResetMask >> 0) & 1;
		_objectCounter[0].broad_pixel *= (_upcomingEvents[_upcomingEventsPointer].pixelCounterResetMask >> 0) & 1;
		_objectCounter[1].pixel *= (_upcomingEvents[_upcomingEventsPointer].pixelCounterResetMask >> 1) & 1;
		_objectCounter[1].broad_pixel *= (_upcomingEvents[_upcomingEventsPointer].pixelCounterResetMask >> 1) & 1;
		_objectCounter[2].pixel *= (_upcomingEvents[_upcomingEventsPointer].pixelCounterResetMask >> 2) & 1;
		_objectCounter[3].pixel *= (_upcomingEvents[_upcomingEventsPointer].pixelCounterResetMask >> 3) & 1;
		_objectCounter[4].pixel *= (_upcomingEvents[_upcomingEventsPointer].pixelCounterResetMask >> 4) & 1;

		// reload the playfield pixel if appropriate
		if(_upcomingEvents[_upcomingEventsPointer].updates & Event::Action::Playfield)
		{
			_playfieldOutput = _upcomingEvents[_upcomingEventsPointer].playfieldPixel;
		}

		// read that state
		state = _upcomingEvents[_upcomingEventsPointer].state;
		OutputState actingState = state;

		// honour the vertical blank flag
		if(_vBlankEnabled && state == OutputState::Pixel) {
			actingState = OutputState::Blank;
		}

		// decide what that means needs to be communicated to the CRT
		_lastOutputStateDuration++;
		if(actingState != _lastOutputState) {
			switch(_lastOutputState) {
				case OutputState::Blank:		_crt->output_blank(_lastOutputStateDuration);				break;
				case OutputState::Sync:			_crt->output_sync(_lastOutputStateDuration);				break;
				case OutputState::ColourBurst:	_crt->output_colour_burst(_lastOutputStateDuration, 96, 0);	break;
				case OutputState::Pixel:		_crt->output_data(_lastOutputStateDuration,	1);				break;
			}
			_lastOutputStateDuration = 0;
			_lastOutputState = actingState;

			if(actingState == OutputState::Pixel) {
				_outputBuffer = _crt->allocate_write_area(160);
			} else {
				_outputBuffer = nullptr;
			}
		}

		// decide on a pixel colour if that's what's happening
		if(state == OutputState::Pixel)
		{
			uint8_t colour = get_output_pixel();
			if(_outputBuffer)
			{
				*_outputBuffer = colour;
				_outputBuffer++;
			}
		}

		// advance
		_upcomingEvents[_upcomingEventsPointer].updates = 0;
		_upcomingEvents[_upcomingEventsPointer].pixelCounterResetMask = ~0;
		_upcomingEventsPointer = (_upcomingEventsPointer + 1)%number_of_upcoming_events;

		// advance horizontal timer, perform reset actions if requested
		_horizontalTimer = (_horizontalTimer + 1) % horizontalTimerPeriod;
		if(!_horizontalTimer)
		{
			_vBlankExtend = false;
			set_ready_line(false);
		}
	}
}

unsigned int Machine::perform_bus_operation(CPU6502::BusOperation operation, uint16_t address, uint8_t *value)
{
	set_reset_line(false);

	uint8_t returnValue = 0xff;
	unsigned int cycles_run_for = 1;
	unsigned int additional_pixels = 0;

	// this occurs as a feedback loop — the 2600 requests ready, then performs the cycles_run_for
	// leap to the end of ready only once ready is signalled — because on a 6502 ready doesn't take
	// effect until the next read; therefore it isn't safe to assume that signalling ready immediately
	// skips to the end of the line.
	if(operation == CPU6502::BusOperation::Ready) {
		unsigned int distance_to_end_of_ready = horizontalTimerPeriod - _horizontalTimer;
		cycles_run_for = distance_to_end_of_ready / 3;
		additional_pixels = distance_to_end_of_ready % 3;
	}

	output_pixels(additional_pixels + cycles_run_for * 3);

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
		if((address&0x1000) && isReadOperation(operation)) {
			returnValue &= _romPages[(address >> 10)&3][address&1023];
		}

		// check for a RAM access
		if((address&0x1280) == 0x80) {
			if(isReadOperation(operation)) {
				returnValue &= _ram[address&0x7f];
			} else {
				_ram[address&0x7f] = *value;
			}
		}

		// check for a TIA access
		if(!(address&0x1080)) {
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
//						printf("%d\n", _horizontalTimer);
//						printf("W");
						if(_horizontalTimer) set_ready_line(true);
					break;
					case 0x03:
						// Reset is delayed by four cycles.
						_horizontalTimer = horizontalTimerPeriod - 4;
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
					case 0x14:
						_objectCounter[decodedAddress - 0x10].count = 0;
					break;

					case 0x1c:
						_ballGraphicsEnable[1] = _ballGraphicsEnable[0];
					case 0x1b: {
						int index = decodedAddress - 0x1b;
						_playerGraphics[0][index] = *value;
						_playerGraphics[1][index^1] = _playerGraphics[0][index^1];
					} break;
					case 0x1d: _missileGraphicsEnable[0] = *value;	break;
					case 0x1e: _missileGraphicsEnable[1] = *value;	break;
					case 0x1f:
						_ballGraphicsEnable[0] = *value;
					break;

					case 0x20:
					case 0x21:
					case 0x22:
					case 0x23:
					case 0x24:
						_objectCounter[decodedAddress - 0x20].motion = *value;
					break;

					case 0x25: _playerGraphicsSelector[0]	= (*value)&1;	break;
					case 0x26: _playerGraphicsSelector[1]	= (*value)&1;	break;
					case 0x27: _ballGraphicsSelector		= (*value)&1;	break;

					case 0x28:
					case 0x29:
					{
						int index = decodedAddress - 0x28;
						if(!(*value&0x02) && _missileGraphicsReset[index])
						{
							_objectCounter[index + 2].count = _objectCounter[index].count;

							uint8_t repeatMask = _playerAndMissileSize[index] & 7;
							int extra_offset;
							switch(repeatMask)
							{
								default:	extra_offset = 3;	break;
								case 5:		extra_offset = 6;	break;
								case 7:		extra_offset = 10;	break;
							}

							_objectCounter[index + 2].count = (_objectCounter[index + 2].count + extra_offset)%160;
						}
						_missileGraphicsReset[index] = (*value) & 0x02;
					}
					break;

					case 0x2a:
						// justification for +5: "we need to wait at least 71 [clocks] before the HMOVE operation is complete";
						// which will take 16*4 + 2 = 66 cycles from the first compare, implying the first compare must be
						// in five cycles from now
						_upcomingEvents[(_upcomingEventsPointer + 5)%number_of_upcoming_events].updates |= Event::Action::HMoveSetup;
					break;
					case 0x2b:
						_objectCounter[0].motion =
						_objectCounter[1].motion =
						_objectCounter[2].motion =
						_objectCounter[3].motion =
						_objectCounter[4].motion = 0;
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
		if((address&0x1280) == 0x280) {
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
						printf("!!!DDR!!!");
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

//	output_pixels(additional_pixels + cycles_run_for * 3);

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
