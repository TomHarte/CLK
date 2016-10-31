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
	// Vertical: 0–39: pixels; otherwise blank; 48–53 sync, 54–56 colour burst
	// Horizontal: 0–223: pixels; otherwise blank; 256–259 sync

#define clamp(action)	\
	if(cycles_run_for <= number_of_cycles) { action; } else cycles_run_for = number_of_cycles;

	while(number_of_cycles)
	{
		int h_counter =_counter & 63;
		int cycles_run_for = 0;

		if(_counter >= _v_sync_start_position && _counter < _v_sync_end_position)
		{
			// this is a sync line
			cycles_run_for = _v_sync_end_position - _counter;
			clamp(_crt->output_sync((unsigned int)(_v_sync_end_position - _v_sync_start_position) * 6));
		}
		else if(_counter < 224*64 && h_counter < 40)
		{
			// this is a pixel line
			if(!h_counter)
			{
				_ink = 0xff;
				_paper = 0x00;
				_use_alternative_character_set = _use_double_height_characters = _blink_text = false;
				set_character_set_base_address();
				_phase += 64;
				_pixel_target = _crt->allocate_write_area(120);

				if(!_counter)
				{
					_phase += 128; // TODO: incorporate all the lines that were missed
					_frame_counter++;

					_v_sync_start_position = _next_frame_is_sixty_hertz ? PAL60VSyncStartPosition : PAL50VSyncStartPosition;
					_v_sync_end_position = _next_frame_is_sixty_hertz ? PAL60VSyncEndPosition : PAL50VSyncEndPosition;
					_counter_period = _next_frame_is_sixty_hertz ? PAL60Period : PAL50Period;
				}
			}

			cycles_run_for = std::min(40 - h_counter, number_of_cycles);
			int columns = cycles_run_for;
			int pixel_base_address = 0xa000 + (_counter >> 6) * 40;
			int character_base_address = 0xbb80 + (_counter >> 9) * 40;
			uint8_t blink_mask = (_blink_text && (_frame_counter&32)) ? 0x00 : 0xff;

			while(columns--)
			{
				uint8_t pixels, control_byte;

				if(_is_graphics_mode && _counter < 200*64)
				{
					control_byte = pixels = _ram[pixel_base_address + h_counter];
				}
				else
				{
					int address = character_base_address + h_counter;
					control_byte = _ram[address];
					int line = _use_double_height_characters ? ((_counter >> 7) & 7) : ((_counter >> 6) & 7);
					pixels = _ram[_character_set_base_address + (control_byte&127) * 8 + line];
				}

				uint8_t inverse_mask = (control_byte & 0x80) ? 0x77 : 0x00;
				pixels &= blink_mask;

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
				h_counter++;
			}

			if(h_counter == 40)
			{
				_crt->output_data(40 * 6, 2);
			}
		}
		else
		{
			// this is a blank line (or the equivalent part of a pixel line)
			if(h_counter < 48)
			{
				cycles_run_for = 48 - h_counter;
				clamp(
					int period = (_counter < 224*64) ? 8 : 48;
					_crt->output_blank((unsigned int)period * 6);
				);
			}
			else if(h_counter < 54)
			{
				cycles_run_for = 54 - h_counter;
				clamp(_crt->output_sync(6 * 6));
			}
			else if(h_counter < 56)
			{
				cycles_run_for = 56 - h_counter;
				clamp(_crt->output_colour_burst(2 * 6, _phase, 128));
			}
			else
			{
				cycles_run_for = 64 - h_counter;
				clamp(_crt->output_blank(8 * 6));
			}
		}

		_counter = (_counter + cycles_run_for)%_counter_period;
		number_of_cycles -= cycles_run_for;
	}
}

void VideoOutput::set_character_set_base_address()
{
	if(_is_graphics_mode) _character_set_base_address = _use_alternative_character_set ? 0x9c00 : 0x9800;
	else _character_set_base_address = _use_alternative_character_set ? 0xb800 : 0xb400;
}
