//
//  ScanTarget.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/11/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#include "ScanTarget.hpp"

using namespace Outputs::Display::OpenGL;

namespace {

const int WriteAreaWidth = 2048;
const int WriteAreaHeight = 2048;

}

void ScanTarget::set_modals(Modals modals) {
	// TODO: consider resizing the write_area_texture_, and setting
	// write_area_texture_line_length_ appropriately.
	modals_ = modals;

	const auto data_type_size = Outputs::Display::size_for_data_type(modals.input_data_type);
	if(data_type_size != data_type_size_) {
		// TODO: flush output.

		data_type_size_ = data_type_size;
		write_area_texture_.resize(2048*2048*data_type_size_);
		write_area_x_ = 0;
		write_area_pointers_.write_pointer = 0;
	}
}

Outputs::Display::ScanTarget::Scan *ScanTarget::get_scan() {
	if(allocation_has_failed_) return nullptr;

	const auto result = &scan_buffer_[scan_buffer_pointers_.write_pointer];

	// Advance the pointer.
	const auto next_write_pointer = (scan_buffer_pointers_.write_pointer + 1) % scan_buffer_.size();

	// Check whether that's too many.
	if(next_write_pointer == scan_buffer_pointers_.read_pointer) {
		allocation_has_failed_ = true;
		return nullptr;
	}
	scan_buffer_pointers_.write_pointer = next_write_pointer;

	// Fill in extra OpenGL-specific details.
	result->data_y = write_area_pointers_.write_pointer;
	result->composite_y = 0;

	return static_cast<Outputs::Display::ScanTarget::Scan *>(result);
}

uint8_t *ScanTarget::allocate_write_area(size_t required_length, size_t required_alignment) {
	if(allocation_has_failed_) return nullptr;

	// Will this fit on the current line? If so, job done.
	uint16_t aligned_start = write_area_x_ + 1;
	aligned_start += uint16_t((required_alignment - aligned_start%required_alignment)%required_alignment);
	const uint16_t end =
		aligned_start
		+ uint16_t(2 + required_length);
	if(end <= WriteAreaWidth) {
		last_supplied_x_ = aligned_start;
		return &write_area_texture_[write_area_pointers_.write_pointer*WriteAreaHeight + aligned_start];
	}

	// Otherwise, look for the next line. But if that's where the read pointer is, don't proceed.
	const uint16_t next_y = (write_area_pointers_.write_pointer + 1) % WriteAreaHeight;
	if(next_y == write_area_pointers_.read_pointer) {
		allocation_has_failed_ = true;
		return nullptr;
	}

	// Advance then.
	last_supplied_x_ = uint16_t(required_alignment);
	write_area_pointers_.write_pointer = next_y;
	return &write_area_texture_[write_area_pointers_.write_pointer*WriteAreaHeight + last_supplied_x_];
}

void ScanTarget::reduce_previous_allocation_to(size_t actual_length) {
	if(allocation_has_failed_) return;

	write_area_x_ = 2 + uint16_t(actual_length) + last_supplied_x_;
}

void ScanTarget::submit() {
	// TODO.
}
