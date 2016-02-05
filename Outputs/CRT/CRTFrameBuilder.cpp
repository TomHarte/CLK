//
//  CRTFrameBuilder.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/02/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "CRT.hpp"

using namespace Outputs;

CRT::CRTFrameBuilder::CRTFrameBuilder(uint16_t width, uint16_t height, unsigned int number_of_buffers, va_list buffer_sizes)
{
	frame.size.width = width;
	frame.size.height = height;
	frame.number_of_buffers = number_of_buffers;
	frame.buffers = new CRTBuffer[number_of_buffers];
	frame.size_per_vertex = kCRTSizeOfVertex;
	frame.geometry_mode = CRTGeometryModeTriangles;

	for(int buffer = 0; buffer < number_of_buffers; buffer++)
	{
		frame.buffers[buffer].depth = va_arg(buffer_sizes, unsigned int);
		frame.buffers[buffer].data = new uint8_t[width * height * frame.buffers[buffer].depth];
	}

	reset();
}

CRT::CRTFrameBuilder::~CRTFrameBuilder()
{
	for(int buffer = 0; buffer < frame.number_of_buffers; buffer++)
		delete[] frame.buffers[buffer].data;
	delete frame.buffers;
}

void CRT::CRTFrameBuilder::reset()
{
	frame.number_of_vertices = 0;
	_next_write_x_position = _next_write_y_position = 0;
	frame.dirty_size.width = 0;
	frame.dirty_size.height = 1;
}

void CRT::CRTFrameBuilder::complete()
{
	frame.vertices = &_all_runs[0];
}

uint8_t *CRT::CRTFrameBuilder::get_next_run()
{
	const size_t vertices_per_run = 6;

	// get a run from the allocated list, allocating more if we're about to overrun
	if((frame.number_of_vertices + vertices_per_run) * frame.size_per_vertex >= _all_runs.size())
	{
		_all_runs.resize(_all_runs.size() + frame.size_per_vertex * vertices_per_run * 100);
	}

	uint8_t *next_run = &_all_runs[frame.number_of_vertices * frame.size_per_vertex];
	frame.number_of_vertices += vertices_per_run;

	return next_run;
}

void CRT::CRTFrameBuilder::allocate_write_area(int required_length)
{
	if (_next_write_x_position + required_length > frame.size.width)
	{
		_next_write_x_position = 0;
		_next_write_y_position = (_next_write_y_position+1)&(frame.size.height-1);
		frame.dirty_size.height++;
	}

	_write_x_position = _next_write_x_position;
	_write_y_position = _next_write_y_position;
	_write_target_pointer = (_write_y_position * frame.size.width) + _write_x_position;
	_next_write_x_position += required_length;
	frame.dirty_size.width = std::max(frame.dirty_size.width, _next_write_x_position);
}

uint8_t *CRT::CRTFrameBuilder::get_write_target_for_buffer(int buffer)
{
	return &frame.buffers[buffer].data[_write_target_pointer * frame.buffers[buffer].depth];
}
