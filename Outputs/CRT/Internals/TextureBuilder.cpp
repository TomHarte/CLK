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

static const GLint internalFormatForDepth(size_t depth) {
	switch(depth) {
		default: return GL_FALSE;
		case 1: return GL_R8UI;
		case 2: return GL_RG8UI;
		case 3: return GL_RGB8UI;
		case 4: return GL_RGBA8UI;
	}
}

static const GLenum formatForDepth(size_t depth) {
	switch(depth) {
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
		first_unsubmitted_y_(0),
		is_full_(false),
		number_of_write_areas_(0) {
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

TextureBuilder::~TextureBuilder() {
	glDeleteTextures(1, &texture_name_);
}

inline uint8_t *TextureBuilder::pointer_to_location(uint16_t x, uint16_t y) {
	return &image_[((y * InputBufferBuilderWidth) + x) * bytes_per_pixel_];
}

uint8_t *TextureBuilder::allocate_write_area(size_t required_length) {
	// Keep a flag to indicate whether the buffer was full at allocate_write_area; if it was then
	// don't return anything now, and decline to act upon follow-up methods. is_full_ may be reset
	// by asynchronous calls to submit. was_full_ will not be touched by it.
	was_full_ = is_full_;
	if(is_full_) return nullptr;

	// If there's not enough space on this line, move to the next. If the next is where the current
	// submission group started, trigger is/was_full_ and return nothing.
	if(write_areas_start_x_ + required_length + 2 > InputBufferBuilderWidth) {
		write_areas_start_x_ = 0;
		write_areas_start_y_ = (write_areas_start_y_ + 1) % InputBufferBuilderHeight;

		if(write_areas_start_y_ == first_unsubmitted_y_) {
			was_full_ = is_full_ = true;
			return nullptr;
		}
	}

	// Queue up the latest write area.
	write_area_.x = write_areas_start_x_ + 1;
	write_area_.y = write_areas_start_y_;
	write_area_.length = (uint16_t)required_length;

	// Return a video pointer.
	return pointer_to_location(write_area_.x, write_area_.y);
}

void TextureBuilder::reduce_previous_allocation_to(size_t actual_length) {
	// If the previous allocate_write_area declined to act, decline also.
	if(was_full_) return;

	// Update the length of the current write area.
	write_area_.length = (uint16_t)actual_length;

	// Bookend the allocation with duplicates of the first and last pixel, to protect
	// against rounding errors when this run is drawn.
	// TODO: allow somebody else to specify the rule for generating a left-padding value and
	// a right-padding value.
	uint8_t *start_pointer = pointer_to_location(write_area_.x, write_area_.y) - bytes_per_pixel_;
	memcpy(	start_pointer,
			&start_pointer[bytes_per_pixel_],
			bytes_per_pixel_);

	memcpy(	&start_pointer[(actual_length + 1) * bytes_per_pixel_],
			&start_pointer[actual_length * bytes_per_pixel_],
			bytes_per_pixel_);
}

bool TextureBuilder::retain_latest() {
	// If the previous allocate_write_area declined to act, decline also.
	if(was_full_) return false;

	// Account for the most recently written area as taken.
	write_areas_start_x_ += write_area_.length + 2;

	// Store into the vector directly if there's already room, otherwise grow the vector.
	// Probably I don't need to mess about with this myself; it's unnecessary second-guessing.
	// TODO: profile and prove.
	if(number_of_write_areas_ < write_areas_.size())
		write_areas_[number_of_write_areas_] = write_area_;
	else
		write_areas_.push_back(write_area_);
	number_of_write_areas_++;

	return true;
}

void TextureBuilder::discard_latest() {
	if(was_full_) return;
	number_of_write_areas_--;
}

bool TextureBuilder::is_full() {
	return is_full_;
}

void TextureBuilder::submit() {
	if(write_areas_start_y_ < first_unsubmitted_y_) {
		// A write area start y less than the first line on which submissions began implies it must have wrapped
		// around. So the submission set is everything back to zero before the current write area plus everything
		// from the first unsubmitted y downward.
		uint16_t height = write_areas_start_y_ + (write_areas_start_x_ ? 1 : 0);
		glTexSubImage2D(	GL_TEXTURE_2D, 0,
							0, 0,
							InputBufferBuilderWidth, height,
							formatForDepth(bytes_per_pixel_), GL_UNSIGNED_BYTE,
							image_.data());

		glTexSubImage2D(	GL_TEXTURE_2D, 0,
							0, first_unsubmitted_y_,
							InputBufferBuilderWidth, InputBufferBuilderHeight - first_unsubmitted_y_,
							formatForDepth(bytes_per_pixel_), GL_UNSIGNED_BYTE,
							image_.data() + first_unsubmitted_y_ * bytes_per_pixel_ * InputBufferBuilderWidth);
	} else {
		// If the current write area start y is after the first unsubmitted line, just submit the region in between.
		uint16_t height = write_areas_start_y_ + (write_areas_start_x_ ? 1 : 0) - first_unsubmitted_y_;
		glTexSubImage2D(	GL_TEXTURE_2D, 0,
							0, first_unsubmitted_y_,
							InputBufferBuilderWidth, height,
							formatForDepth(bytes_per_pixel_), GL_UNSIGNED_BYTE,
							image_.data() + first_unsubmitted_y_ * bytes_per_pixel_ * InputBufferBuilderWidth);
	}

	// Update the starting location for the next submission, and mark definitively that the buffer is once again not full.
	first_unsubmitted_y_ = write_areas_start_y_;
	is_full_ = false;
}

void TextureBuilder::flush(const std::function<void(const std::vector<WriteArea> &write_areas, size_t count)> &function) {
	// Just throw everything currently in the flush queue to the provided function, and note that
	// the queue is now empty.
	if(number_of_write_areas_) {
		function(write_areas_, number_of_write_areas_);
	}
	number_of_write_areas_ = 0;
}
