//
//  6560.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/06/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "6560.hpp"

using namespace MOS;

MOS6560::MOS6560() :
	_crt(new Outputs::CRT::CRT(65 * 4, 4, Outputs::CRT::DisplayType::NTSC60, 1))
{
	_crt->set_rgb_sampling_function(
		"vec3 rgb_sample(usampler2D sampler, vec2 coordinate, vec2 icoordinate)"
		"{"
			"return vec3(1.0);"
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

		case 0xf:
			// TODO: colours
		break;

		default:
		break;
	}

	printf("%02x: %02x\n", address, value);
	printf("%04x %04x [%d by %d from %d, %d]\n", _character_cell_start_address, _video_matrix_start_address, _number_of_columns, _number_of_rows, _first_column_location, _first_row_location);
}

uint16_t MOS6560::get_address()
{
	return 0x1c;
}

void MOS6560::set_graphics_value(uint8_t value)
{
}
