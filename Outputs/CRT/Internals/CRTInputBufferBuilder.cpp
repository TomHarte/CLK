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

CRTInputBufferBuilder::CRTInputBufferBuilder(unsigned int number_of_buffers, va_list buffer_sizes)
{
	this->number_of_buffers = number_of_buffers;
	buffers = new CRTInputBufferBuilder::Buffer[number_of_buffers];

	for(int buffer = 0; buffer < number_of_buffers; buffer++)
	{
		buffers[buffer].bytes_per_pixel = va_arg(buffer_sizes, unsigned int);
		buffers[buffer].data = new uint8_t[InputBufferBuilderWidth * InputBufferBuilderHeight * buffers[buffer].bytes_per_pixel];
	}

	_next_write_x_position = _next_write_y_position = 0;
	last_uploaded_line = 0;
}

CRTInputBufferBuilder::~CRTInputBufferBuilder()
{
	for(int buffer = 0; buffer < number_of_buffers; buffer++)
		delete[] buffers[buffer].data;
	delete buffers;
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

void CRTInputBufferBuilder::reduce_previous_allocation_to(size_t actual_length)
{
	// book end the allocation with duplicates of the first and last pixel, to protect
	// against rounding errors when this run is drawn
	for(int c = 0; c < number_of_buffers; c++)
	{
		memcpy(	&buffers[c].data[(_write_target_pointer - 1) * buffers[c].bytes_per_pixel],
				&buffers[c].data[_write_target_pointer * buffers[c].bytes_per_pixel],
				buffers[c].bytes_per_pixel);

		memcpy(	&buffers[c].data[(_write_target_pointer + actual_length) * buffers[c].bytes_per_pixel],
				&buffers[c].data[(_write_target_pointer + actual_length - 1) * buffers[c].bytes_per_pixel],
				buffers[c].bytes_per_pixel);
	}

	// return any allocated length that wasn't actually used to the available pool
	_next_write_x_position -= (_last_allocation_amount - actual_length);
}

uint8_t *CRTInputBufferBuilder::get_write_target_for_buffer(int buffer)
{
	return &buffers[buffer].data[_write_target_pointer * buffers[buffer].bytes_per_pixel];
}
