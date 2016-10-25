//
//  Video.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 12/10/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Video.hpp"

using namespace Oric;

namespace {
	const unsigned int PAL50VSyncStartPosition = 256*64;
	const unsigned int PAL60VSyncStartPosition = 234*64;
	const unsigned int PAL50VSyncEndPosition = 259*64;
	const unsigned int PAL60VSyncEndPosition = 238*64;
	const unsigned int PAL50Period = 312*64;
	const unsigned int PAL60Period = 262*64;
}

VideoOutput::VideoOutput(uint8_t *memory) :
	_ram(memory),
	_frame_counter(0), _counter(0),
	_state(Sync), _cycles_in_state(0),
	_is_graphics_mode(false),
	_character_set_base_address(0xb400),
	_phase(0),
	_v_sync_start_position(PAL50VSyncStartPosition), _v_sync_end_position(PAL50VSyncEndPosition),
	_counter_period(PAL50Period), _next_frame_is_sixty_hertz(false)
{
	_crt.reset(new Outputs::CRT::CRT(64*6, 6, Outputs::CRT::DisplayType::PAL50, 1));

	// TODO: this is a copy and paste from the Electron; factor out.
	_crt->set_rgb_sampling_function(
		"vec3 rgb_sample(usampler2D sampler, vec2 coordinate, vec2 icoordinate)"
		"{"
			"uint texValue = texture(sampler, coordinate).r;"
			"texValue >>= 4 - (int(icoordinate.x * 8) & 4);"
			"return vec3( uvec3(texValue) & uvec3(4u, 2u, 1u));"
		"}");

	_crt->set_output_device(Outputs::CRT::Television);
	_crt->set_visible_area(_crt->get_rect_for_area(50, 224, 16 * 6, 40 * 6, 4.0f / 3.0f));
}

std::shared_ptr<Outputs::CRT::CRT> VideoOutput::get_crt()
{
	return _crt;
}

void VideoOutput::run_for_cycles(int number_of_cycles)
{
	// Vertical: 0–39: pixels; otherwise blank; 48–53 sync
	// Horizontal: 0–223: pixels; otherwise blank; 256–259 sync

	while(number_of_cycles--)
	{
		_counter = (_counter + 1)%_counter_period;
		int h_counter =_counter & 63;

		if(!h_counter)
		{
			_ink = 0xff;
			_paper = 0x00;
			_use_alternative_character_set = _use_double_height_characters = _blink_text = false;
			set_character_set_base_address();
			_phase += 64;

			if(!_counter)
			{
				_phase += 128;
				_frame_counter++;

				_v_sync_start_position = _next_frame_is_sixty_hertz ? PAL60VSyncStartPosition : PAL50VSyncStartPosition;
				_v_sync_end_position = _next_frame_is_sixty_hertz ? PAL60VSyncEndPosition : PAL50VSyncEndPosition;
				_counter_period = _next_frame_is_sixty_hertz ? PAL60Period : PAL50Period;
			}
		}

		State new_state = Blank;
		if(
			(h_counter >= 48 && h_counter <= 53) ||
			(_counter >= _v_sync_start_position && _counter <= _v_sync_end_position)) new_state = Sync;
		else if(h_counter >= 54 && h_counter <= 56) new_state = ColourBurst;
		else if(_counter < 224*64 && h_counter < 40) new_state = Pixels;

		if(_state != new_state)
		{
			switch(_state)
			{
				case ColourBurst:	_crt->output_colour_burst(_cycles_in_state * 6, _phase, 128);	break;
				case Sync:			_crt->output_sync(_cycles_in_state * 6);						break;
				case Blank:			_crt->output_blank(_cycles_in_state * 6);						break;
				case Pixels:		_crt->output_data(_cycles_in_state * 6, 2);						break;
			}
			_state = new_state;
			_cycles_in_state = 0;
			if(_state == Pixels) _pixel_target = _crt->allocate_write_area(120);
		}
		_cycles_in_state++;

		if(new_state == Pixels) {
			uint8_t pixels, control_byte;

			if(_is_graphics_mode && _counter < 200*64)
			{
				control_byte = pixels = _ram[0xa000 + (_counter >> 6) * 40 + h_counter];
			}
			else
			{
				int address = 0xbb80 + (_counter >> 9) * 40 + h_counter;
				control_byte = _ram[address];
				int line = _use_double_height_characters ? ((_counter >> 7) & 7) : ((_counter >> 6) & 7);
				pixels = _ram[_character_set_base_address + (control_byte&127) * 8 + line];
			}

			uint8_t inverse_mask = (control_byte & 0x80) ? 0x77 : 0x00;
			if(_blink_text && (_frame_counter&32)) pixels = 0;

			if(control_byte & 0x60)
			{
				if(_pixel_target)
				{
					uint8_t colours[2] = {
						(uint8_t)(_paper ^ inverse_mask),
						(uint8_t)(_ink ^ inverse_mask),
					};

					_pixel_target[0] = (colours[(pixels >> 4)&1] & 0x0f) | (colours[(pixels >> 5)&1] & 0xf0);
					_pixel_target[1] = (colours[(pixels >> 2)&1] & 0x0f) | (colours[(pixels >> 3)&1] & 0xf0);
					_pixel_target[2] = (colours[(pixels >> 0)&1] & 0x0f) | (colours[(pixels >> 1)&1] & 0xf0);
				}
			}
			else
			{
				switch(control_byte & 0x1f)
				{
					case 0x00:		_ink = 0x00;	break;
					case 0x01:		_ink = 0x44;	break;
					case 0x02:		_ink = 0x22;	break;
					case 0x03:		_ink = 0x66;	break;
					case 0x04:		_ink = 0x11;	break;
					case 0x05:		_ink = 0x55;	break;
					case 0x06:		_ink = 0x33;	break;
					case 0x07:		_ink = 0x77;	break;

					case 0x08:	case 0x09:	case 0x0a: case 0x0b:
					case 0x0c:	case 0x0d:	case 0x0e: case 0x0f:
						_use_alternative_character_set = (control_byte&1);
						_use_double_height_characters = (control_byte&2);
						_blink_text = (control_byte&4);
						set_character_set_base_address();
					break;

					case 0x10:		_paper = 0x00;	break;
					case 0x11:		_paper = 0x44;	break;
					case 0x12:		_paper = 0x22;	break;
					case 0x13:		_paper = 0x66;	break;
					case 0x14:		_paper = 0x11;	break;
					case 0x15:		_paper = 0x55;	break;
					case 0x16:		_paper = 0x33;	break;
					case 0x17:		_paper = 0x77;	break;

					case 0x18: case 0x19: case 0x1a: case 0x1b:
					case 0x1c: case 0x1d: case 0x1e: case 0x1f:
						_is_graphics_mode = (control_byte & 4);
						_next_frame_is_sixty_hertz = !(control_byte & 2);
					break;

					default: break;
				}
				if(_pixel_target) _pixel_target[0] = _pixel_target[1] = _pixel_target[2] = (uint8_t)(_paper ^ inverse_mask);
			}
			if(_pixel_target) _pixel_target += 3;
		}
	}
}
