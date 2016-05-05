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

CRTInputBufferBuilder::CRTInputBufferBuilder(size_t bytes_per_pixel) :
	_bytes_per_pixel(bytes_per_pixel),
	_next_write_x_position(0),
	_next_write_y_position(0),
	_last_uploaded_line(0),
	_is_full(false),
	_image(new uint8_t[bytes_per_pixel * InputBufferBuilderWidth * InputBufferBuilderHeight])
{}

CRTInputBufferBuilder::~CRTInputBufferBuilder()
{
	delete[] _image;
}

void CRTInputBufferBuilder::allocate_write_area(size_t required_length)
{
	if(!_is_full)
	{
		_last_allocation_amount = required_length;

		if(_next_write_x_position + required_length + 2 > InputBufferBuilderWidth)
		{
			_next_write_x_position = 0;
			_next_write_y_position++;

			_is_full = (_next_write_y_position == _last_uploaded_line + InputBufferBuilderHeight);
			if(_is_full)
				return;
		}

		_write_x_position = _next_write_x_position + 1;
		_write_y_position = _next_write_y_position;
		_write_target_pointer = (_write_y_position * InputBufferBuilderWidth) + _write_x_position;
		_next_write_x_position += required_length + 2;
	}
}

bool CRTInputBufferBuilder::reduce_previous_allocation_to(size_t actual_length)
{
	if(_is_full) return false;

	// book end the allocation with duplicates of the first and last pixel, to protect
	// against rounding errors when this run is drawn
	memcpy(	&_image[(_write_target_pointer - 1) * _bytes_per_pixel],
			&_image[_write_target_pointer * _bytes_per_pixel],
			_bytes_per_pixel);

	memcpy(	&_image[(_write_target_pointer + actual_length) * _bytes_per_pixel],
			&_image[(_write_target_pointer + actual_length - 1) * _bytes_per_pixel],
			_bytes_per_pixel);

	// return any allocated length that wasn't actually used to the available pool
	_next_write_x_position -= (_last_allocation_amount - actual_length);

	return true;
}

uint8_t *CRTInputBufferBuilder::get_image_pointer()
{
	return _image;
}

uint16_t CRTInputBufferBuilder::get_and_finalise_current_line()
{
	uint16_t result = _write_y_position;
	_next_write_x_position = _next_write_y_position = 0;
	_is_full = false;
	return result;
}

uint8_t *CRTInputBufferBuilder::get_write_target()
{
	return _is_full ? nullptr : &_image[_write_target_pointer * _bytes_per_pixel];
}

uint16_t CRTInputBufferBuilder::get_last_write_x_position()
{
	return _write_x_position;
}

uint16_t CRTInputBufferBuilder::get_last_write_y_position()
{
	return _write_y_position % InputBufferBuilderHeight;
}

size_t CRTInputBufferBuilder::get_bytes_per_pixel()
{
	return _bytes_per_pixel;
}
