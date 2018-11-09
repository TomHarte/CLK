//
//  ScanTarget.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/11/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#include "ScanTarget.hpp"
#include "Primitives/Rectangle.hpp"

using namespace Outputs::Display::OpenGL;

namespace {

constexpr int WriteAreaWidth = 2048;
constexpr int WriteAreaHeight = 2048;

#define TextureAddress(x, y) (((x) << 11) | (y))
#define TextureAddressGetX(v) ((v) >> 11)
#define TextureAddressGetY(v) ((v) & 0x7ff)
#define TextureSub(x, y) (((x) - (y)) & 0x7ff)

}

ScanTarget::ScanTarget() {
	// Allocate space for the spans.
	glGenBuffers(1, &scan_buffer_name_);
	glBindBuffer(GL_ARRAY_BUFFER, scan_buffer_name_);
	const auto buffer_size = scan_buffer_.size() * sizeof(Scan);
	glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(buffer_size), NULL, GL_STREAM_DRAW);
}

ScanTarget::~ScanTarget() {
	// Release span space.
	glDeleteBuffers(1, &scan_buffer_name_);
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

		write_pointers_.scan_buffer = 0;
		write_pointers_.write_area = 0;
	}
}

Outputs::Display::ScanTarget::Scan *ScanTarget::get_scan() {
	if(allocation_has_failed_) return nullptr;

	const auto result = &scan_buffer_[write_pointers_.scan_buffer];
	const auto read_pointers = read_pointers_.load();

	// Advance the pointer.
	const auto next_write_pointer = decltype(write_pointers_.scan_buffer)((write_pointers_.scan_buffer + 1) % scan_buffer_.size());

	// Check whether that's too many.
	if(next_write_pointer == read_pointers.scan_buffer) {
		allocation_has_failed_ = true;
		return nullptr;
	}
	write_pointers_.scan_buffer = next_write_pointer;

	// Fill in extra OpenGL-specific details.
//	result->data_y = write_pointers_.write_area;
	result->composite_y = 0;

	return static_cast<Outputs::Display::ScanTarget::Scan *>(result);
}

uint8_t *ScanTarget::allocate_write_area(size_t required_length, size_t required_alignment) {
	if(allocation_has_failed_) return nullptr;

	// Determine where the proposed write area would start and end.
	uint16_t output_y = TextureAddressGetY(write_pointers_.write_area);

	uint16_t aligned_start_x = TextureAddressGetX(write_pointers_.write_area & 0xffff) + 1;
	aligned_start_x += uint16_t((required_alignment - aligned_start_x%required_alignment)%required_alignment);

	uint16_t end_x = aligned_start_x + uint16_t(1 + required_length);

	if(end_x > WriteAreaWidth) {
		output_y = (output_y + 1) % WriteAreaHeight;
		aligned_start_x = uint16_t(required_alignment);
		end_x = aligned_start_x + uint16_t(1 + required_length);
	}

	// Check whether that steps over the read pointer.
	const auto new_address = TextureAddress(end_x, output_y);
	const auto read_pointers = read_pointers_.load();

	const auto new_distance = TextureSub(read_pointers.write_area, new_address);
	const auto previous_distance = TextureSub(read_pointers.write_area, write_pointers_.write_area);

	// If allocating this would somehow make the write pointer further away from the read pointer,
	// there must not be enough space left.
	if(new_distance > previous_distance) {
		allocation_has_failed_ = true;
		return nullptr;
	}

	// Everything checks out, return the pointer.
	last_supplied_x_ = aligned_start_x;
	return &write_area_texture_[size_t(new_address) * data_type_size_];
}

void ScanTarget::reduce_previous_allocation_to(size_t actual_length) {
	if(allocation_has_failed_) return;

	// The span was allocated in the knowledge that there's sufficient distance
	// left on the current line, so there's no need to worry about carry.
	write_pointers_.write_area += actual_length + 1;
}

void ScanTarget::submit() {
	if(allocation_has_failed_) {
		// Reset all pointers to where they were.
		write_pointers_ = submit_pointers_.load();
	} else {
		// Advance submit pointer.
		submit_pointers_.store(write_pointers_);
	}

	allocation_has_failed_ = false;
}

void ScanTarget::draw() {
	// Grab the current read and submit pointers.
	const auto submit_pointers = submit_pointers_.load();
	const auto read_pointers = read_pointers_.load();

	// Submit spans.
	if(submit_pointers.scan_buffer != read_pointers.scan_buffer) {

//		uint8_t *destination = static_cast<uint8_t *>(glMapBufferRange(GL_ARRAY_BUFFER, 0, (GLsizeiptr)length, GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT | GL_MAP_FLUSH_EXPLICIT_BIT));
//		if(!glGetError() && destination) {
//		}

		// TODO: submit all scans from scan_buffer_pointers_.read_pointer to scan_buffer_pointers_.submit_pointer.
//		read_pointers_.scan_buffer = submit_pointers.scan_buffer;
	}

	// All data now having been spooled to the GPU, update the read pointers to
	// the submit pointer location.
	read_pointers_.store(submit_pointers);

	glClear(GL_COLOR_BUFFER_BIT);
//	::OpenGL::Rectangle rect(-0.8f, -0.8f, 1.6f, 1.6f);
//	rect.draw(1, 1, 0);
}
