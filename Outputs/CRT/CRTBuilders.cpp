//
//  CRTFrameBuilder.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/02/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "CRT.hpp"
#include "CRTOpenGL.hpp"

using namespace Outputs;

/*
	CRTInputBufferBuilder
*/

CRT::CRTInputBufferBuilder::CRTInputBufferBuilder(unsigned int number_of_buffers, va_list buffer_sizes)
{
	this->number_of_buffers = number_of_buffers;
	buffers = new CRTInputBufferBuilder::Buffer[number_of_buffers];

	for(int buffer = 0; buffer < number_of_buffers; buffer++)
	{
		buffers[buffer].bytes_per_pixel = va_arg(buffer_sizes, unsigned int);
		buffers[buffer].data = new uint8_t[CRTInputBufferBuilderWidth * CRTInputBufferBuilderHeight * buffers[buffer].bytes_per_pixel];
	}

	_next_write_x_position = _next_write_y_position = 0;
	last_uploaded_line = 0;
}

CRT::CRTInputBufferBuilder::~CRTInputBufferBuilder()
{
	for(int buffer = 0; buffer < number_of_buffers; buffer++)
		delete[] buffers[buffer].data;
	delete buffers;
}


void CRT::CRTInputBufferBuilder::allocate_write_area(size_t required_length)
{
	_last_allocation_amount = required_length;

	if(_next_write_x_position + required_length + 2 > CRTInputBufferBuilderWidth)
	{
		_next_write_x_position = 0;
		_next_write_y_position = (_next_write_y_position+1)%CRTInputBufferBuilderHeight;
	}

	_write_x_position = _next_write_x_position + 1;
	_write_y_position = _next_write_y_position;
	_write_target_pointer = (_write_y_position * CRTInputBufferBuilderWidth) + _write_x_position;
	_next_write_x_position += required_length + 2;
}

void CRT::CRTInputBufferBuilder::reduce_previous_allocation_to(size_t actual_length)
{
	for(int c = 0; c < number_of_buffers; c++)
	{
		memcpy(	&buffers[c].data[(_write_target_pointer - 1) * buffers[c].bytes_per_pixel],
				&buffers[c].data[_write_target_pointer * buffers[c].bytes_per_pixel],
				buffers[c].bytes_per_pixel);

		memcpy(	&buffers[c].data[(_write_target_pointer + actual_length) * buffers[c].bytes_per_pixel],
				&buffers[c].data[(_write_target_pointer + actual_length - 1) * buffers[c].bytes_per_pixel],
				buffers[c].bytes_per_pixel);
	}

	_next_write_x_position -= (_last_allocation_amount - actual_length);
}


uint8_t *CRT::CRTInputBufferBuilder::get_write_target_for_buffer(int buffer)
{
	return &buffers[buffer].data[_write_target_pointer * buffers[buffer].bytes_per_pixel];
}

/*
	CRTRunBuilder
*/
void CRT::CRTRunBuilder::reset()
{
	number_of_vertices = 0;
}

uint8_t *CRT::CRTRunBuilder::get_next_input_run()
{
	const size_t vertices_per_run = 6;

	// get a run from the allocated list, allocating more if we're about to overrun
	if((number_of_vertices + vertices_per_run) * kCRTSizeOfVertex >= _input_runs.size())
	{
		_input_runs.resize(_input_runs.size() + kCRTSizeOfVertex * vertices_per_run * 100);
	}

	uint8_t *next_run = &_input_runs[number_of_vertices * kCRTSizeOfVertex];
	number_of_vertices += vertices_per_run;

	return next_run;
}
