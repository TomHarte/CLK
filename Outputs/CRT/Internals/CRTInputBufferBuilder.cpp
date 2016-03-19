//
//  CRTInputBufferBuilder.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 08/03/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "CRTInputBufferBuilder.hpp"
#include "CRTOpenGL.hpp"
#include <string.h>

using namespace Outputs::CRT;

CRTInputBufferBuilder::CRTInputBufferBuilder(size_t bytes_per_pixel) : bytes_per_pixel(bytes_per_pixel)
{
	_next_write_x_position = _next_write_y_position = 0;
	last_uploaded_line = 0;
}

void CRTInputBufferBuilder::allocate_write_area(size_t required_length)
{
	_last_allocation_amount = required_length;

	if(_next_write_x_position + required_length + 2 > InputBufferBuilderWidth)
	{
		move_to_new_line();
	}

	_write_x_position = _next_write_x_position + 1;
	_write_y_position = _next_write_y_position;
	_write_target_pointer = (_write_y_position * InputBufferBuilderWidth) + _write_x_position;
	_next_write_x_position += required_length + 2;
}

void CRTInputBufferBuilder::reduce_previous_allocation_to(size_t actual_length, uint8_t *buffer)
{
	// book end the allocation with duplicates of the first and last pixel, to protect
	// against rounding errors when this run is drawn
	memcpy(	&buffer[(_write_target_pointer - 1) * bytes_per_pixel],
			&buffer[_write_target_pointer * bytes_per_pixel],
			bytes_per_pixel);

	memcpy(	&buffer[(_write_target_pointer + actual_length) * bytes_per_pixel],
			&buffer[(_write_target_pointer + actual_length - 1) * bytes_per_pixel],
			bytes_per_pixel);

	// return any allocated length that wasn't actually used to the available pool
	_next_write_x_position -= (_last_allocation_amount - actual_length);
}
