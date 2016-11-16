//
//  InputTextureBuilder.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 08/03/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "InputTextureBuilder.hpp"
#include "CRTOpenGL.hpp"
#include <string.h>

using namespace Outputs::CRT;

InputTextureBuilder::InputTextureBuilder(size_t bytes_per_pixel) :
	_bytes_per_pixel(bytes_per_pixel),
	_next_write_x_position(0),
	_next_write_y_position(0)
{
	_image.resize(bytes_per_pixel * InputBufferBuilderWidth * InputBufferBuilderHeight);
}

uint8_t *InputTextureBuilder::allocate_write_area(size_t required_length)
{
	if(_next_write_y_position != InputBufferBuilderHeight)
	{
		_last_allocation_amount = required_length;

		if(_next_write_x_position + required_length + 2 > InputBufferBuilderWidth)
		{
			_next_write_x_position = 0;
			_next_write_y_position++;

			if(_next_write_y_position == InputBufferBuilderHeight)
				return nullptr;
		}

		_write_x_position = _next_write_x_position + 1;
		_write_y_position = _next_write_y_position;
		_write_target_pointer = (_write_y_position * InputBufferBuilderWidth) + _write_x_position;
		_next_write_x_position += required_length + 2;
	}
	else return nullptr;

	return &_image[_write_target_pointer * _bytes_per_pixel];
}

bool InputTextureBuilder::is_full()
{
	return (_next_write_y_position == InputBufferBuilderHeight);
}

void InputTextureBuilder::reduce_previous_allocation_to(size_t actual_length)
{
	if(_next_write_y_position == InputBufferBuilderHeight) return;

	uint8_t *const image_pointer = _image.data();

	// correct if the writing cursor was reset while a client was writing
	if(_next_write_x_position == 0 && _next_write_y_position == 0)
	{
		memmove(&image_pointer[_bytes_per_pixel], &image_pointer[_write_target_pointer * _bytes_per_pixel], actual_length * _bytes_per_pixel);
		_write_target_pointer = 1;
		_last_allocation_amount = actual_length;
		_next_write_x_position = (uint16_t)(actual_length + 2);
		_write_x_position = 1;
		_write_y_position = 0;
	}

	// book end the allocation with duplicates of the first and last pixel, to protect
	// against rounding errors when this run is drawn
	memcpy(	&image_pointer[(_write_target_pointer - 1) * _bytes_per_pixel],
			&image_pointer[_write_target_pointer * _bytes_per_pixel],
			_bytes_per_pixel);

	memcpy(	&image_pointer[(_write_target_pointer + actual_length) * _bytes_per_pixel],
			&image_pointer[(_write_target_pointer + actual_length - 1) * _bytes_per_pixel],
			_bytes_per_pixel);

	// return any allocated length that wasn't actually used to the available pool
	_next_write_x_position -= (_last_allocation_amount - actual_length);
}

uint8_t *InputTextureBuilder::get_image_pointer()
{
	return _image.data();
}

uint16_t InputTextureBuilder::get_and_finalise_current_line()
{
	uint16_t result = _write_y_position + (_next_write_x_position ? 1 : 0);
	_next_write_x_position = _next_write_y_position = 0;
	return result;
}

uint16_t InputTextureBuilder::get_last_write_x_position()
{
	return _write_x_position;
}

uint16_t InputTextureBuilder::get_last_write_y_position()
{
	return _write_y_position;
}

size_t InputTextureBuilder::get_bytes_per_pixel()
{
	return _bytes_per_pixel;
}
