//
//  6560.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/06/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "6560.hpp"

using namespace MOS;

/*
     0 - 0000   Black
     1 - 0001   White
     2 - 0010   Red
     3 - 0011   Cyan
     4 - 0100   Purple
     5 - 0101   Green
     6 - 0110   Blue
     7 - 0111   Yellow

     8 - 1000   Orange
     9 - 1001   Light orange
    10 - 1010   Pink
    11 - 1011   Light cyan
    12 - 1100   Light purple
    13 - 1101   Light green
    14 - 1110   Light blue
    15 - 1111   Light yellow
*/

MOS6560::MOS6560() :
	_crt(new Outputs::CRT::CRT(65*4, 4, 261, Outputs::CRT::ColourSpace::YIQ, 65*4, 1, 1)),
	_horizontal_counter(0),
	_vertical_counter(0)
{
	_crt->set_composite_sampling_function(
		"float composite_sample(usampler2D texID, vec2 coordinate, vec2 iCoordinate, float phase, float amplitude)"
		"{"
			"uint c = texture(texID, coordinate).r;"
			"float y = 0.75 + (float(c & 8u) / 8.0) * 0.25 * step(1, c);"

			"uint iPhase = c & 7u;"
			"float phaseOffset = float(iPhase + 3u) / 7.0;"	// TODO: appropriate phaseOffset

			"return mix(step(1, c) * y, step(2, c) * step(mod(phase + phaseOffset, 6.283185308), 3.141592654), amplitude);"
		"}");
}

void MOS6560::set_register(int address, uint8_t value)
{
	switch(address)
	{
		case 0x0:
			_interlaced = !!(value&0x80);
			_first_column_location = value & 0x7f;
		break;

		case 0x1:
			_first_row_location = value;
		break;

		case 0x2:
			_number_of_columns = value & 0x7f;
			_video_matrix_start_address = (uint16_t)((_video_matrix_start_address & 0x3c00) | ((value & 0x80) << 2));
		break;

		case 0x3:
			_number_of_rows = (value >> 1)&0x3f;
			_wide_characters = !!(value&0x01);
		break;

		case 0x5:
			_character_cell_start_address = (uint16_t)((value & 0x0f) << 10);
			_video_matrix_start_address = (uint16_t)((_video_matrix_start_address & 0x0400) | ((value & 0xf0) << 5));
		break;

		case 0xe:
			_auxiliary_colour = value >> 4;
			// TODO: sound amplitude
		break;

		case 0xf:
			if(_this_state == State::Border)
			{
				output_border(_cycles_in_state * 4);
				_cycles_in_state = 0;
			}
			_invertedCells = !!((value >> 3)&1);
			_borderColour = value & 0x07;
			_backgroundColour = value >> 4;
		break;

		// TODO: audio, primarily

		default:
		break;
	}
}

void MOS6560::output_border(unsigned int number_of_cycles)
{
	uint8_t *colour_pointer = _crt->allocate_write_area(1);
	if(colour_pointer) *colour_pointer = _borderColour;
	_crt->output_level(number_of_cycles);
}

uint16_t MOS6560::get_address()
{
	_horizontal_counter++;
	if(_horizontal_counter == 65)
	{
		_horizontal_counter = 0;
		_vertical_counter++;
		_column_counter = -1;

		if(_vertical_counter == 261)
		{
			_vertical_counter = 0;
			_row_counter = -1;
		}

		if(_row_counter >= 0)
		{
			_row_counter++;
			if(_row_counter == _number_of_rows*8) _row_counter = -1;
		}
		else if(_vertical_counter >= _first_row_location * 2)
		{
			_video_matrix_line_address_counter = _video_matrix_start_address;
			_row_counter = 0;
		}
	}

	if(_column_counter >= 0)
	{
		_column_counter++;
		if(_column_counter == _number_of_columns*2)
		{
			_column_counter = -1;
			if((_row_counter&7) == 7)
			{
				_video_matrix_line_address_counter = _video_matrix_address_counter;
			}
		}
	}
	else if(_horizontal_counter == _first_column_location)
	{
		_column_counter = 0;
		_video_matrix_address_counter = _video_matrix_line_address_counter;
	}

	// determine output state; colour burst and sync timing are currently a guess
	if(_horizontal_counter > 61) _this_state = State::ColourBurst;
	else if(_horizontal_counter > 57) _this_state = State::Sync;
	else
	{
		_this_state = (_column_counter >= 0 && _row_counter >= 0) ? State::Pixels : State::Border;
	}

	// update the CRT
	if(_this_state != _output_state)
	{
		switch(_output_state)
		{
			case State::Sync:			_crt->output_sync(_cycles_in_state * 4);				break;
			case State::ColourBurst:	_crt->output_colour_burst(_cycles_in_state * 4, 0, 0);	break;
			case State::Border:			output_border(_cycles_in_state * 4);					break;
			case State::Pixels:			_crt->output_data(_cycles_in_state * 4, 1);				break;
		}
		_output_state = _this_state;
		_cycles_in_state = 0;

		pixel_pointer = nullptr;
		if(_output_state == State::Pixels)
		{
			pixel_pointer = _crt->allocate_write_area(260);
		}
	}
	_cycles_in_state++;

	// compute the address
	if(_this_state == State::Pixels)
	{
		/*
			Per http://tinyvga.com/6561 :

			The basic video timing is very simple.  For
			every character the VIC-I is about to display, it first fetches the
			character code and colour, then the character appearance (from the
			character generator memory).  The character codes are read on every
			raster line, thus making every line a "bad line".  When the raster
			beam is outside of the text window, the videochip reads from $001c for
			most time.  (Some videochips read from $181c instead.)  The address
			occasionally varies, but it might also be due to a flaky bus.  (By
			reading from unconnected address space, such as $9100-$910f, you can
			read the data fetched by the videochip on the previous clock cycle.)
		*/
		if(_column_counter&1)
		{
			// TODO: don't add 0x8000. That's a hack.
			return 0x8000 + (_character_cell_start_address + (_character_code*8) + (_row_counter&7));
		}
		else
		{
			uint16_t result = _video_matrix_address_counter;
			_video_matrix_address_counter++;
			return result;
		}
	}

	return 0x1c;
}

void MOS6560::set_graphics_value(uint8_t value, uint8_t colour_value)
{
	// TODO: this isn't correct, as _character_value will be
	// accessed second, then output will roll over. Probably it's
	// correct (given the delays upstream) to output all 8 on an &1 
	// and to adjust the signalling to the CRT above?
	if(_this_state == State::Pixels)
	{
		if(_column_counter&1)
		{
			_character_value = value;

			if(pixel_pointer)
			{
				uint8_t cell_colour = _character_colour & 0x7;
				if(!(_character_colour&0x8))
				{
					pixel_pointer[0] = ((_character_value >> 7)&1) ? cell_colour : _backgroundColour;
					pixel_pointer[1] = ((_character_value >> 6)&1) ? cell_colour : _backgroundColour;
					pixel_pointer[2] = ((_character_value >> 5)&1) ? cell_colour : _backgroundColour;
					pixel_pointer[3] = ((_character_value >> 4)&1) ? cell_colour : _backgroundColour;
					pixel_pointer[4] = ((_character_value >> 3)&1) ? cell_colour : _backgroundColour;
					pixel_pointer[5] = ((_character_value >> 2)&1) ? cell_colour : _backgroundColour;
					pixel_pointer[6] = ((_character_value >> 1)&1) ? cell_colour : _backgroundColour;
					pixel_pointer[7] = ((_character_value >> 0)&1) ? cell_colour : _backgroundColour;
				}
				else
				{
					uint8_t colours[4] = {_backgroundColour, _borderColour, cell_colour, _auxiliary_colour};
					pixel_pointer[0] =
					pixel_pointer[1] = colours[(_character_value >> 6)&3];
					pixel_pointer[2] =
					pixel_pointer[3] = colours[(_character_value >> 4)&3];
					pixel_pointer[4] =
					pixel_pointer[5] = colours[(_character_value >> 2)&3];
					pixel_pointer[6] =
					pixel_pointer[7] = colours[(_character_value >> 0)&3];
				}
				pixel_pointer += 8;
			}
		}
		else
		{
			_character_code = value;
			_character_colour = colour_value;
		}
	}
}
