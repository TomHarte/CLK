//
//  TextureBuilder.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 08/03/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "TextureBuilder.hpp"
#include "CRTOpenGL.hpp"
#include "OpenGL.hpp"
#include <string.h>

using namespace Outputs::CRT;

static const GLint internalFormatForDepth(size_t depth)
{
	switch(depth)
	{
		default: return GL_FALSE;
		case 1: return GL_R8UI;
		case 2: return GL_RG8UI;
		case 3: return GL_RGB8UI;
		case 4: return GL_RGBA8UI;
	}
}

static const GLenum formatForDepth(size_t depth)
{
	switch(depth)
	{
		default: return GL_FALSE;
		case 1: return GL_RED_INTEGER;
		case 2: return GL_RG_INTEGER;
		case 3: return GL_RGB_INTEGER;
		case 4: return GL_RGBA_INTEGER;
	}
}

TextureBuilder::TextureBuilder(size_t bytes_per_pixel, GLenum texture_unit) :
	bytes_per_pixel_(bytes_per_pixel),
	next_write_x_position_(0),
	next_write_y_position_(0)
{
	image_.resize(bytes_per_pixel * InputBufferBuilderWidth * InputBufferBuilderHeight);
	glGenTextures(1, &texture_name_);

	glActiveTexture(texture_unit);
	glBindTexture(GL_TEXTURE_2D, texture_name_);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, internalFormatForDepth(bytes_per_pixel), InputBufferBuilderWidth, InputBufferBuilderHeight, 0, formatForDepth(bytes_per_pixel), GL_UNSIGNED_BYTE, nullptr);
}

TextureBuilder::~TextureBuilder()
{
	glDeleteTextures(1, &texture_name_);
}

uint8_t *TextureBuilder::allocate_write_area(size_t required_length)
{
	if(next_write_y_position_ != InputBufferBuilderHeight)
	{
		last_allocation_amount_ = required_length;

		if(next_write_x_position_ + required_length + 2 > InputBufferBuilderWidth)
		{
			next_write_x_position_ = 0;
			next_write_y_position_++;

			if(next_write_y_position_ == InputBufferBuilderHeight)
				return nullptr;
		}

		write_x_position_ = next_write_x_position_ + 1;
		write_y_position_ = next_write_y_position_;
		write_target_pointer_ = (write_y_position_ * InputBufferBuilderWidth) + write_x_position_;
		next_write_x_position_ += required_length + 2;
	}
	else return nullptr;

	return &image_[write_target_pointer_ * bytes_per_pixel_];
}

bool TextureBuilder::is_full()
{
	return (next_write_y_position_ == InputBufferBuilderHeight);
}

void TextureBuilder::reduce_previous_allocation_to(size_t actual_length)
{
	if(next_write_y_position_ == InputBufferBuilderHeight) return;

	uint8_t *const image_pointer = image_.data();

	// correct if the writing cursor was reset while a client was writing
	if(next_write_x_position_ == 0 && next_write_y_position_ == 0)
	{
		memmove(&image_pointer[bytes_per_pixel_], &image_pointer[write_target_pointer_ * bytes_per_pixel_], actual_length * bytes_per_pixel_);
		write_target_pointer_ = 1;
		last_allocation_amount_ = actual_length;
		next_write_x_position_ = (uint16_t)(actual_length + 2);
		write_x_position_ = 1;
		write_y_position_ = 0;
	}

	// book end the allocation with duplicates of the first and last pixel, to protect
	// against rounding errors when this run is drawn
	memcpy(	&image_pointer[(write_target_pointer_ - 1) * bytes_per_pixel_],
			&image_pointer[write_target_pointer_ * bytes_per_pixel_],
			bytes_per_pixel_);

	memcpy(	&image_pointer[(write_target_pointer_ + actual_length) * bytes_per_pixel_],
			&image_pointer[(write_target_pointer_ + actual_length - 1) * bytes_per_pixel_],
			bytes_per_pixel_);

	// return any allocated length that wasn't actually used to the available pool
	next_write_x_position_ -= (last_allocation_amount_ - actual_length);
}

uint16_t TextureBuilder::get_last_write_x_position()
{
	return write_x_position_;
}

uint16_t TextureBuilder::get_last_write_y_position()
{
	return write_y_position_;
}

void TextureBuilder::submit()
{
	uint16_t height = write_y_position_ + (next_write_x_position_ ? 1 : 0);
	next_write_x_position_ = next_write_y_position_ = 0;

	glTexSubImage2D(	GL_TEXTURE_2D, 0,
						0, 0,
						InputBufferBuilderWidth, height,
						formatForDepth(bytes_per_pixel_), GL_UNSIGNED_BYTE,
						image_.data());
}
