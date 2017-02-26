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
	write_areas_start_x_(0),
	write_areas_start_y_(0),
	is_full_(false),
	did_submit_(false),
	has_write_area_(false),
	number_of_write_areas_(0)
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

inline uint8_t *TextureBuilder::pointer_to_location(uint16_t x, uint16_t y)
{
	return &image_[((y * InputBufferBuilderWidth) + x) * bytes_per_pixel_];
}

uint8_t *TextureBuilder::allocate_write_area(size_t required_length)
{
	if(is_full_) return nullptr;

	uint16_t starting_x, starting_y;

	if(!number_of_write_areas_)
	{
		starting_x = write_areas_start_x_;
		starting_y = write_areas_start_y_;
	}
	else
	{
		starting_x = write_areas_[number_of_write_areas_ - 1].x + write_areas_[number_of_write_areas_ - 1].length + 1;
		starting_y = write_areas_[number_of_write_areas_ - 1].y;
	}

	WriteArea next_write_area;
	if(starting_x + required_length + 2 > InputBufferBuilderWidth)
	{
		starting_x = 0;
		starting_y++;

		if(starting_y == InputBufferBuilderHeight)
		{
			is_full_ = true;
			return nullptr;
		}
	}

	next_write_area.x = starting_x + 1;
	next_write_area.y = starting_y;
	next_write_area.length = (uint16_t)required_length;
	if(number_of_write_areas_ < write_areas_.size())
		write_areas_[number_of_write_areas_] = next_write_area;
	else
		write_areas_.push_back(next_write_area);
	number_of_write_areas_++;
	has_write_area_ = true;

	return pointer_to_location(next_write_area.x, next_write_area.y);
}

bool TextureBuilder::is_full()
{
	return is_full_;
}

void TextureBuilder::reduce_previous_allocation_to(size_t actual_length)
{
	if(is_full_ || !has_write_area_) return;

	has_write_area_ = false;
	WriteArea &write_area = write_areas_[number_of_write_areas_-1];
	write_area.length = (uint16_t)actual_length;

	// book end the allocation with duplicates of the first and last pixel, to protect
	// against rounding errors when this run is drawn
	uint8_t *start_pointer = pointer_to_location(write_area.x, write_area.y);
	memcpy(	&start_pointer[-bytes_per_pixel_],
			start_pointer,
			bytes_per_pixel_);

	memcpy(	&start_pointer[actual_length * bytes_per_pixel_],
			&start_pointer[(actual_length - 1) * bytes_per_pixel_],
			bytes_per_pixel_);
}

void TextureBuilder::submit()
{
	uint16_t height = write_areas_start_y_ + (write_areas_start_x_ ? 1 : 0);
	did_submit_ = true;

	glTexSubImage2D(	GL_TEXTURE_2D, 0,
						0, 0,
						InputBufferBuilderWidth, height,
						formatForDepth(bytes_per_pixel_), GL_UNSIGNED_BYTE,
						image_.data());
}

void TextureBuilder::flush(const std::function<void(const std::vector<WriteArea> &write_areas, size_t count)> &function)
{
	bool was_full = is_full_;
	if(did_submit_)
	{
		write_areas_start_y_ = write_areas_start_x_ = 0;
		is_full_ = false;
	}

	if(number_of_write_areas_ && !was_full)
	{
		if(write_areas_[0].x != write_areas_start_x_+1 || write_areas_[0].y != write_areas_start_y_)
		{
			for(size_t area = 0; area < number_of_write_areas_; area++)
			{
				WriteArea &write_area = write_areas_[area];

				if(write_areas_start_x_ + write_area.length + 2 > InputBufferBuilderWidth)
				{
					write_areas_start_x_ = 0;
					write_areas_start_y_ ++;

					if(write_areas_start_y_ == InputBufferBuilderHeight)
					{
						is_full_ = true;
						break;
					}
				}

				memmove(
					pointer_to_location(write_areas_start_x_, write_areas_start_y_),
					pointer_to_location(write_area.x - 1, write_area.y),
					(write_area.length + 2) * bytes_per_pixel_);
				write_area.x = write_areas_start_x_ + 1;
				write_area.y = write_areas_start_y_;
			}
		}

		if(!is_full_)
		{
			function(write_areas_, number_of_write_areas_);

			write_areas_start_x_ = write_areas_[number_of_write_areas_-1].x + write_areas_[number_of_write_areas_-1].length + 1;
			write_areas_start_y_ = write_areas_[number_of_write_areas_-1].y;
		}
	}

	did_submit_ = false;
	has_write_area_ = false;
	number_of_write_areas_ = 0;
}
