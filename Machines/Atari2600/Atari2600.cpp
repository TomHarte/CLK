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
	_rom(nullptr),
	_tiaInputValue{0xff, 0xff},
	_upcomingEventsPointer(0),
	_objectCounterPointer(0),
	_stateByTime(_stateByExtendTime[0]),
	_cycles_since_speaker_update(0)
{
	memset(_collisions, 0xff, sizeof(_collisions));
	set_reset_line(true);
	setup_reported_collisions();

	for(int vbextend = 0; vbextend < 2; vbextend++)
	{
		for(int c = 0; c < 57; c++)
		{
			OutputState state;

			// determine which output state will be active in four cycles from now
			switch(c)
			{
				case 0: case 1: case 2: case 3:					state = OutputState::Blank;									break;
				case 4: case 5: case 6: case 7:					state = OutputState::Sync;									break;
				case 8: case 9: case 10: case 11:				state = OutputState::ColourBurst;							break;
				case 12: case 13: case 14:
				case 15: case 16:								state = OutputState::Blank;									break;

				case 17: case 18:								state = vbextend ? OutputState::Blank : OutputState::Pixel;	break;
				default:										state = OutputState::Pixel;									break;
			}

			_stateByExtendTime[vbextend][c] = state;
		}
	}
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
			"return mix(float(y) / 14.0, step(1, iPhase) * cos(phase + phaseOffset), amplitude);"
		"}");
	_crt->set_output_device(Outputs::CRT::Television);

	_speaker.set_input_rate(1194720 / 38);
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
			"return mix(float(y) / 14.0, step(4, (iPhase + 2u) & 15u) * cos(phase + phaseOffset), amplitude);"
		"}");
	_crt->set_new_timing(228, 312, Outputs::CRT::ColourSpace::YUV, 228, 1);

//	_speaker.set_input_rate(2 * 312 * 50);
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

	_objectCounterPointer = (_objectCounterPointer + 1)%number_of_recorded_counters;
	ObjectCounter *oneClockAgo = _objectCounter[(_objectCounterPointer - 1 + number_of_recorded_counters)%number_of_recorded_counters];
	ObjectCounter *twoClocksAgo = _objectCounter[(_objectCounterPointer - 2 + number_of_recorded_counters)%number_of_recorded_counters];
	ObjectCounter *now = _objectCounter[_objectCounterPointer];

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
		// is the result of a counter rollover or a programmatic reset, and there's a four
		// clock delay on that triggering the start signal
		now[4].count = (oneClockAgo[4].count + 1)%160;
		now[4].pixel = oneClockAgo[4].pixel + 1;
		if(!now[4].count) now[4].pixel = 0;
	}
	else
	{
		now[4] = oneClockAgo[4];
	}

	// check for player and missle triggers
	for(int c = 0; c < 4; c++)
	{
		if(mask & (1 << c))
		{
			// update the count
			now[c].count = (oneClockAgo[c].count + 1)%160;

			uint8_t repeatMask = _playerAndMissileSize[c&1] & 7;
			ObjectCounter *rollover;
			ObjectCounter *equality;

			if(c < 2)
			{
				// update the pixel
				now[c].broad_pixel = oneClockAgo[c].broad_pixel + 1;
				switch(repeatMask)
				{
					default:	now[c].pixel = oneClockAgo[c].pixel + 1;	break;
					case 5:		now[c].pixel = oneClockAgo[c].pixel + (now[c].broad_pixel&1);	break;
					case 7:		now[c].pixel = oneClockAgo[c].pixel + (((now[c].broad_pixel | (now[c].broad_pixel >> 1))^1)&1);	break;
				}

				// check for a rollover six clocks ago or equality five clocks ago
				rollover = twoClocksAgo;
				equality = oneClockAgo;
			}
			else
			{
				// update the pixel
				now[c].pixel = oneClockAgo[c].pixel + 1;

				// check for a rollover five clocks ago or equality four clocks ago
				rollover = oneClockAgo;
				equality = now;
			}

			if(
				(rollover[c].count == 159) ||
				(_hasSecondCopy[c&1] && equality[c].count == 16) ||
				(_hasThirdCopy[c&1] && equality[c].count == 32) ||
				(_hasFourthCopy[c&1] && equality[c].count == 64)
			)
			{
				now[c].pixel = 0;
				now[c].broad_pixel = 0;
			}
		}
		else
		{
			now[c] = oneClockAgo[c];
		}
	}
}

uint8_t Machine::get_output_pixel()
{
	ObjectCounter *now = _objectCounter[_objectCounterPointer];

	// get the playfield pixel
	unsigned int offset = _horizontalTimer - (horizontalTimerPeriod - 160);
	uint8_t playfieldColour = ((_playfieldControl&6) == 2) ? _playerColour[offset / 80] : _playfieldColour;

	// ball pixel
	uint8_t ballPixel = 0;
	if(now[4].pixel < _ballSize) {
		ballPixel = _ballGraphicsEnable[_ballGraphicsSelector];
	}

	// determine the player and missile pixels
	uint8_t playerPixels[2] = { 0, 0 };
	uint8_t missilePixels[2] = { 0, 0 };
	for(int c = 0; c < 2; c++)
	{
		if(_playerGraphics[c] && now[c].pixel < 8) {
			playerPixels[c] = (_playerGraphics[_playerGraphicsSelector[c]][c] >> (now[c].pixel ^ _playerReflectionMask[c])) & 1;
		}

		if(!_missileGraphicsReset[c] && now[c+2].pixel < _missileSize[c]) {
			missilePixels[c] = _missileGraphicsEnable[c];
		}
	}

	// accumulate collisions
	int pixel_mask = playerPixels[0] | (playerPixels[1] << 1) | (missilePixels[0] << 2) | (missilePixels[1] << 3) | (ballPixel << 4) | (_playfieldOutput << 5);
	_collisions[0] |= _reportedCollisions[pixel_mask][0];
	_collisions[1] |= _reportedCollisions[pixel_mask][1];
	_collisions[2] |= _reportedCollisions[pixel_mask][2];
	_collisions[3] |= _reportedCollisions[pixel_mask][3];
	_collisions[4] |= _reportedCollisions[pixel_mask][4];
	_collisions[5] |= _reportedCollisions[pixel_mask][5];
	_collisions[6] |= _reportedCollisions[pixel_mask][6];
	_collisions[7] |= _reportedCollisions[pixel_mask][7];

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

void Machine::setup_reported_collisions()
{
	for(int c = 0; c < 64; c++)
	{
		memset(_reportedCollisions[c], 0, 8);

		int playerPixels[2] = { c&1, (c >> 1)&1 };
		int missilePixels[2] = { (c >> 2)&1, (c >> 3)&1 };
		int ballPixel = (c >> 4)&1;
		int playfieldPixel = (c >> 5)&1;

		if(playerPixels[0] | playerPixels[1]) {
			_reportedCollisions[c][0] |= ((missilePixels[0] & playerPixels[1]) << 7)	| ((missilePixels[0] & playerPixels[0]) << 6);
			_reportedCollisions[c][1] |= ((missilePixels[1] & playerPixels[0]) << 7)	| ((missilePixels[1] & playerPixels[1]) << 6);

			_reportedCollisions[c][2] |= ((playfieldPixel & playerPixels[0]) << 7)	| ((ballPixel & playerPixels[0]) << 6);
			_reportedCollisions[c][3] |= ((playfieldPixel & playerPixels[1]) << 7)	| ((ballPixel & playerPixels[1]) << 6);

			_reportedCollisions[c][7] |= ((playerPixels[0] & playerPixels[1]) << 7);
		}

		if(playfieldPixel | ballPixel) {
			_reportedCollisions[c][4] |= ((playfieldPixel & missilePixels[0]) << 7)	| ((ballPixel & missilePixels[0]) << 6);
			_reportedCollisions[c][5] |= ((playfieldPixel & missilePixels[1]) << 7)	| ((ballPixel & missilePixels[1]) << 6);

			_reportedCollisions[c][6] |= ((playfieldPixel & ballPixel) << 7);
		}

		if(missilePixels[0] & missilePixels[1])
			_reportedCollisions[c][7] |= (1 << 6);
	}
}

void Machine::output_pixels(unsigned int count)
{
	while(count--)
	{
		if(_upcomingEvents[_upcomingEventsPointer].updates)
		{
			// apply any queued changes and flush the record
			if(_upcomingEvents[_upcomingEventsPointer].updates & Event::Action::HMoveSetup)
			{
				// schedule an extended left border
				_stateByTime = _stateByExtendTime[1];

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

				// follow-through into a compare immediately
				_upcomingEvents[_upcomingEventsPointer].updates |= Event::Action::HMoveCompare;
			}

			if(_upcomingEvents[_upcomingEventsPointer].updates & Event::Action::HMoveCompare)
			{
				for(int c = 0; c < 5; c++)
				{
					if(((_objectMotion[c] >> 4)^_hMoveCounter) == 7)
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

			if(_upcomingEvents[_upcomingEventsPointer].updates & Event::Action::ResetCounter)
			{
				_objectCounter[_objectCounterPointer][_upcomingEvents[_upcomingEventsPointer].counter].count = 0;
			}

			// zero out current update event
			_upcomingEvents[_upcomingEventsPointer].updates = 0;
		}

		//  progress to next event
		_upcomingEventsPointer = (_upcomingEventsPointer + 1)%number_of_upcoming_events;

		// determine which output state is currently active
		OutputState primary_state = _stateByTime[_horizontalTimer >> 2];
		OutputState effective_state = primary_state;

		// update pixel timers
		if(primary_state == OutputState::Pixel) update_timers(~0);

		// update the background chain
		if(_horizontalTimer >= 64 && _horizontalTimer <= 160+64 && !(_horizontalTimer&3))
		{
			_playfieldOutput = _nextPlayfieldOutput;
			_nextPlayfieldOutput = _playfield[(_horizontalTimer - 64) >> 2];
		}

		// if vsync is enabled, output the opposite of the automatic hsync output;
		// also	honour the vertical blank flag
		if(_vSyncEnabled) {
			effective_state = (effective_state = OutputState::Sync) ? OutputState::Blank : OutputState::Sync;
		} else if(_vBlankEnabled && effective_state == OutputState::Pixel) {
			effective_state = OutputState::Blank;
		}

		// decide what that means needs to be communicated to the CRT
		_lastOutputStateDuration++;
		if(effective_state != _lastOutputState) {
			switch(_lastOutputState) {
				case OutputState::Blank:		_crt->output_blank(_lastOutputStateDuration);				break;
				case OutputState::Sync:			_crt->output_sync(_lastOutputStateDuration);				break;
				case OutputState::ColourBurst:	_crt->output_colour_burst(_lastOutputStateDuration, 96, 0);	break;
				case OutputState::Pixel:		_crt->output_data(_lastOutputStateDuration,	1);				break;
			}
			_lastOutputStateDuration = 0;
			_lastOutputState = effective_state;

			if(effective_state == OutputState::Pixel) {
				_outputBuffer = _crt->allocate_write_area(160);
			} else {
				_outputBuffer = nullptr;
			}
		}

		// decide on a pixel colour if that's what's happening
		if(effective_state == OutputState::Pixel)
		{
			uint8_t colour = get_output_pixel();
			if(_outputBuffer)
			{
				*_outputBuffer = colour;
				_outputBuffer++;
			}
		}

		// advance horizontal timer, perform reset actions if desired
		_horizontalTimer = (_horizontalTimer + 1) % horizontalTimerPeriod;
		if(!_horizontalTimer)
		{
			// switch back to a normal length left border
			_stateByTime = _stateByExtendTime[0];
			set_ready_line(false);
		}
	}
}

unsigned int Machine::perform_bus_operation(CPU6502::BusOperation operation, uint16_t address, uint8_t *value)
{
	set_reset_line(false);

	uint8_t returnValue = 0xff;
	unsigned int cycles_run_for = 3;

	// this occurs as a feedback loop — the 2600 requests ready, then performs the cycles_run_for
	// leap to the end of ready only once ready is signalled — because on a 6502 ready doesn't take
	// effect until the next read; therefore it isn't safe to assume that signalling ready immediately
	// skips to the end of the line.
	if(operation == CPU6502::BusOperation::Ready) {
		unsigned int distance_to_end_of_ready = horizontalTimerPeriod - _horizontalTimer;
		cycles_run_for = distance_to_end_of_ready;
	}

	output_pixels(cycles_run_for);
	_cycles_since_speaker_update += cycles_run_for;

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
				returnValue &= _mos6532.get_ram(address);
			} else {
				_mos6532.set_ram(address, *value);
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
						if(_horizontalTimer) set_ready_line(true);
					break;
					case 0x03:
						// Reset is delayed by four cycles.
						_horizontalTimer = horizontalTimerPeriod - 4;

						// TODO: audio will now be out of synchronisation — fix
					break;

					case 0x04:
					case 0x05: {
						int entry = decodedAddress - 0x04;
						_playerAndMissileSize[entry] = *value;
						_missileSize[entry] = 1 << ((*value >> 4)&3);

						uint8_t repeatMask = (*value)&7;
						_hasSecondCopy[entry] = (repeatMask == 1) || (repeatMask == 3);
						_hasThirdCopy[entry] = (repeatMask == 2) || (repeatMask == 3) || (repeatMask == 6);
						_hasFourthCopy[entry] = (repeatMask == 4) || (repeatMask == 6);
					} break;

					case 0x06:
					case 0x07: _playerColour[decodedAddress - 0x06] = *value;	break;
					case 0x08: _playfieldColour = *value;	break;
					case 0x09: _backgroundColour = *value;	break;

					case 0x0a: {
						uint8_t old_playfield_control = _playfieldControl;
						_playfieldControl = *value;
						_ballSize = 1 << ((_playfieldControl >> 4)&3);

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
					case 0x0c: _playerReflectionMask[decodedAddress - 0x0b] = (*value)&8 ? 0 : 7;	break;

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
						_upcomingEvents[(_upcomingEventsPointer + 4)%number_of_upcoming_events].updates |= Event::Action::ResetCounter;
						_upcomingEvents[(_upcomingEventsPointer + 4)%number_of_upcoming_events].counter = decodedAddress - 0x10;
					break;

					case 0x15: case 0x16:
						update_audio();
						_speaker.set_control(decodedAddress - 0x15, *value);
					break;

					case 0x17: case 0x18:
						update_audio();
						_speaker.set_divider(decodedAddress - 0x17, *value);
					break;

					case 0x19: case 0x1a:
						update_audio();
						_speaker.set_volume(decodedAddress - 0x19, *value);
					break;

					case 0x1c:
						_ballGraphicsEnable[1] = _ballGraphicsEnable[0];
					case 0x1b: {
						int index = decodedAddress - 0x1b;
						_playerGraphics[0][index] = *value;
						_playerGraphics[1][index^1] = _playerGraphics[0][index^1];
					} break;
					case 0x1d:
					case 0x1e:
						_missileGraphicsEnable[decodedAddress - 0x1d] = ((*value) >> 1)&1;
//						printf("e:%02x <- %c\n", decodedAddress - 0x1d, ((*value)&1) ? 'E' : '-');
					break;
					case 0x1f:
						_ballGraphicsEnable[0] = ((*value) >> 1)&1;
					break;

					case 0x20:
					case 0x21:
					case 0x22:
					case 0x23:
					case 0x24:
						_objectMotion[decodedAddress - 0x20] = *value;
					break;

					case 0x25: _playerGraphicsSelector[0]	= (*value)&1;	break;
					case 0x26: _playerGraphicsSelector[1]	= (*value)&1;	break;
					case 0x27: _ballGraphicsSelector		= (*value)&1;	break;

					case 0x28:
					case 0x29:
					{
						// TODO: this should properly mean setting a flag and propagating later, I think?
						int index = decodedAddress - 0x28;
						if(!(*value&0x02) && _missileGraphicsReset[index])
						{
							_objectCounter[_objectCounterPointer][index + 2].count = _objectCounter[_objectCounterPointer][index].count;

							uint8_t repeatMask = _playerAndMissileSize[index] & 7;
							int extra_offset;
							switch(repeatMask)
							{
								default:	extra_offset = 3;	break;
								case 5:		extra_offset = 6;	break;
								case 7:		extra_offset = 10;	break;
							}

							_objectCounter[_objectCounterPointer][index + 2].count = (_objectCounter[_objectCounterPointer][index + 2].count + extra_offset)%160;
						}
						_missileGraphicsReset[index] = !!((*value) & 0x02);
//						printf("r:%02x <- %c\n", decodedAddress - 0x28, ((*value)&2) ? 'R' : '-');
					}
					break;

					case 0x2a: {
						// justification for +5: "we need to wait at least 71 [clocks] before the HMOVE operation is complete";
						// which will take 16*4 + 2 = 66 cycles from the first compare, implying the first compare must be
						// in five cycles from now
//						int start_pause = ((_horizontalTimer + 3)&3) + 4;
						_upcomingEvents[(_upcomingEventsPointer + 5)%number_of_upcoming_events].updates |= Event::Action::HMoveSetup;
					} break;
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
		if((address&0x1280) == 0x280) {
			if(isReadOperation(operation)) {
				returnValue &= _mos6532.get_register(address);
			} else {
				_mos6532.set_register(address, *value);
			}
		}

		if(isReadOperation(operation)) {
			*value = returnValue;
		}
	}

	_mos6532.run_for_cycles(cycles_run_for / 3);

	return cycles_run_for / 3;
}

void Machine::set_digital_input(Atari2600DigitalInput input, bool state)
{
	switch (input) {
		case Atari2600DigitalInputJoy1Up:		_mos6532.update_port_input(0, 0x10, state);	break;
		case Atari2600DigitalInputJoy1Down:		_mos6532.update_port_input(0, 0x20, state);	break;
		case Atari2600DigitalInputJoy1Left:		_mos6532.update_port_input(0, 0x40, state);	break;
		case Atari2600DigitalInputJoy1Right:	_mos6532.update_port_input(0, 0x80, state);	break;

		case Atari2600DigitalInputJoy2Up:		_mos6532.update_port_input(0, 0x01, state);	break;
		case Atari2600DigitalInputJoy2Down:		_mos6532.update_port_input(0, 0x02, state);	break;
		case Atari2600DigitalInputJoy2Left:		_mos6532.update_port_input(0, 0x04, state);	break;
		case Atari2600DigitalInputJoy2Right:	_mos6532.update_port_input(0, 0x08, state);	break;

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

#pragma mark - Audio

void Machine::update_audio()
{
	unsigned int audio_cycles = _cycles_since_speaker_update / 114;

//	static unsigned int total_cycles = 0;
//	total_cycles += audio_cycles;
//	static time_t logged_time = 0;
//	time_t time_now = time(nullptr);
//	if(time_now - logged_time > 0)
//	{
//		printf("[s] %ld : %d\n", time_now - logged_time, total_cycles);
//		total_cycles = 0;
//		logged_time = time_now;
//	}

	_speaker.run_for_cycles(audio_cycles);
	_cycles_since_speaker_update %= 114;
}

void Machine::synchronise()
{
	update_audio();
}

Atari2600::Speaker::Speaker()
{
	_poly4_counter[0] = _poly4_counter[1] = 0x00f;
	_poly5_counter[0] = _poly5_counter[1] = 0x01f;
	_poly9_counter[0] = _poly9_counter[1] = 0x1ff;
}

Atari2600::Speaker::~Speaker()
{
}

void Atari2600::Speaker::set_volume(int channel, uint8_t volume)
{
	_volume[channel] = volume & 0xf;
}

void Atari2600::Speaker::set_divider(int channel, uint8_t divider)
{
	_divider[channel] = divider & 0x1f;
	_divider_counter[channel] = 0;
}

void Atari2600::Speaker::set_control(int channel, uint8_t control)
{
	_control[channel] = control & 0xf;
}

#define advance_poly4(c) _poly4_counter[channel] = (_poly4_counter[channel] >> 1) | (((_poly4_counter[channel] << 3) ^ (_poly4_counter[channel] << 2))&0x008)
#define advance_poly5(c) _poly5_counter[channel] = (_poly5_counter[channel] >> 1) | (((_poly5_counter[channel] << 4) ^ (_poly5_counter[channel] << 2))&0x010)
#define advance_poly9(c) _poly9_counter[channel] = (_poly9_counter[channel] >> 1) | (((_poly9_counter[channel] << 4) ^ (_poly9_counter[channel] << 8))&0x100)


void Atari2600::Speaker::get_samples(unsigned int number_of_samples, int16_t *target)
{
	for(unsigned int c = 0; c < number_of_samples; c++)
	{
		target[c] = 0;
		for(int channel = 0; channel < 2; channel++)
		{
			_divider_counter[channel] ++;
			int level = 0;
			switch(_control[channel])
			{
				case 0x0: case 0xb:	// constant 1
					level = 1;
				break;

				case 0x4: case 0x5:	// div2 tone
					level = (_divider_counter[channel] / (_divider[channel]+1))&1;
				break;

				case 0xc: case 0xd:	// div6 tone
					level = (_divider_counter[channel] / ((_divider[channel]+1)*3))&1;
				break;

				case 0x6: case 0xa:	// div31 tone
					level = (_divider_counter[channel] / (_divider[channel]+1))%30 <= 18;
				break;

				case 0xe:			// div93 tone
					level = (_divider_counter[channel] / ((_divider[channel]+1)*3))%30 <= 18;
				break;

				case 0x1:			// 4-bit poly
					level = _poly4_counter[channel]&1;
					if(_divider_counter[channel] == _divider[channel]+1)
					{
						_divider_counter[channel] = 0;
						advance_poly4(channel);
					}
				break;

				case 0x2:			// 4-bit poly div31
					level = _poly4_counter[channel]&1;
					if(_divider_counter[channel]%(30*(_divider[channel]+1)) == 18)
					{
						advance_poly4(channel);
					}
				break;

				case 0x3:			// 5/4-bit poly
					level = _output_state[channel];
					if(_divider_counter[channel] == _divider[channel]+1)
					{
						if(_poly5_counter[channel]&1)
						{
							_output_state[channel] = _poly4_counter[channel]&1;
							advance_poly4(channel);
						}
						advance_poly5(channel);
					}
				break;

				case 0x7: case 0x9:	// 5-bit poly
					level = _poly5_counter[channel]&1;
					if(_divider_counter[channel] == _divider[channel]+1)
					{
						_divider_counter[channel] = 0;
						advance_poly5(channel);
					}
				break;

				case 0xf:			// 5-bit poly div6
					level = _poly5_counter[channel]&1;
					if(_divider_counter[channel] == (_divider[channel]+1)*3)
					{
						_divider_counter[channel] = 0;
						advance_poly5(channel);
					}
				break;

				case 0x8:			// 9-bit poly
					level = _poly9_counter[channel]&1;
					if(_divider_counter[channel] == _divider[channel]+1)
					{
						_divider_counter[channel] = 0;
						advance_poly9(channel);
					}
				break;
			}

			target[c] += _volume[channel] * 1024 * level;
		}
	}
}

void Atari2600::Speaker::skip_samples(unsigned int number_of_samples)
{
}
