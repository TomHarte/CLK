//
//  BufferingScanTarget.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 22/07/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#include "BufferingScanTarget.hpp"

using namespace Outputs::Display;

BufferingScanTarget::BufferingScanTarget() {
	// Ensure proper initialisation of the two atomic pointer sets.
	read_pointers_.store(write_pointers_);
	submit_pointers_.store(write_pointers_);

	// Establish initial state for is_updating_.
	is_updating_.clear();
}

void BufferingScanTarget::set_modals(Modals modals) {
	// Don't change the modals while drawing is ongoing; a previous set might be
	// in the process of being established.
	while(is_updating_.test_and_set());
	modals_ = modals;
	modals_are_dirty_ = true;
	is_updating_.clear();
}

void BufferingScanTarget::end_scan() {
	if(vended_scan_) {
		std::lock_guard lock_guard(write_pointers_mutex_);
		vended_scan_->data_y = TextureAddressGetY(vended_write_area_pointer_);
		vended_scan_->line = write_pointers_.line;
		vended_scan_->scan.end_points[0].data_offset += TextureAddressGetX(vended_write_area_pointer_);
		vended_scan_->scan.end_points[1].data_offset += TextureAddressGetX(vended_write_area_pointer_);

#ifdef LOG_SCANS
		if(vended_scan_->scan.composite_amplitude) {
			std::cout << "S: ";
			std::cout << vended_scan_->scan.end_points[0].composite_angle << "/" << vended_scan_->scan.end_points[0].data_offset << "/" << vended_scan_->scan.end_points[0].cycles_since_end_of_horizontal_retrace << " -> ";
			std::cout << vended_scan_->scan.end_points[1].composite_angle << "/" << vended_scan_->scan.end_points[1].data_offset << "/" << vended_scan_->scan.end_points[1].cycles_since_end_of_horizontal_retrace << " => ";
			std::cout << double(vended_scan_->scan.end_points[1].composite_angle - vended_scan_->scan.end_points[0].composite_angle) / (double(vended_scan_->scan.end_points[1].data_offset - vended_scan_->scan.end_points[0].data_offset) * 64.0f) << "/";
			std::cout << double(vended_scan_->scan.end_points[1].composite_angle - vended_scan_->scan.end_points[0].composite_angle) / (double(vended_scan_->scan.end_points[1].cycles_since_end_of_horizontal_retrace - vended_scan_->scan.end_points[0].cycles_since_end_of_horizontal_retrace) * 64.0f);
			std::cout << std::endl;
		}
#endif
	}
	vended_scan_ = nullptr;
}

uint8_t *BufferingScanTarget::begin_data(size_t required_length, size_t required_alignment) {
	assert(required_alignment);

	if(allocation_has_failed_) return nullptr;

	std::lock_guard lock_guard(write_pointers_mutex_);
	if(!write_area_) {
		allocation_has_failed_ = true;
		return nullptr;
	}

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
	const auto end_address = TextureAddress(end_x, output_y);
	const auto read_pointers = read_pointers_.load();

	const auto end_distance = TextureSub(end_address, read_pointers.write_area);
	const auto previous_distance = TextureSub(write_pointers_.write_area, read_pointers.write_area);

	// If allocating this would somehow make the write pointer back away from the read pointer,
	// there must not be enough space left.
	if(end_distance < previous_distance) {
		allocation_has_failed_ = true;
		return nullptr;
	}

	// Everything checks out, note expectation of a future end_data and return the pointer.
	data_is_allocated_ = true;
	vended_write_area_pointer_ = write_pointers_.write_area = TextureAddress(aligned_start_x, output_y);

	assert(write_pointers_.write_area >= 1 && ((size_t(write_pointers_.write_area) + required_length + 1) * data_type_size_) <= WriteAreaWidth*WriteAreaHeight*data_type_size_);
	return &write_area_[size_t(write_pointers_.write_area) * data_type_size_];

	// Note state at exit:
	//		write_pointers_.write_area points to the first pixel the client is expected to draw to.
}

void BufferingScanTarget::end_data(size_t actual_length) {
	if(allocation_has_failed_ || !data_is_allocated_) return;

	std::lock_guard lock_guard(write_pointers_mutex_);

	// Bookend the start of the new data, to safeguard for precision errors in sampling.
	memcpy(
		&write_area_[size_t(write_pointers_.write_area - 1) * data_type_size_],
		&write_area_[size_t(write_pointers_.write_area) * data_type_size_],
		data_type_size_);

	// Advance to the end of the current run.
	write_pointers_.write_area += actual_length + 1;

	// Also bookend the end.
	memcpy(
		&write_area_[size_t(write_pointers_.write_area - 1) * data_type_size_],
		&write_area_[size_t(write_pointers_.write_area - 2) * data_type_size_],
		data_type_size_);

	// The write area was allocated in the knowledge that there's sufficient
	// distance left on the current line, but there's a risk of exactly filling
	// the final line, in which case this should wrap back to 0.
	write_pointers_.write_area %= WriteAreaWidth*WriteAreaHeight;

	// Record that no further end_data calls are expected.
	data_is_allocated_ = false;
}

void BufferingScanTarget::will_change_owner() {
	allocation_has_failed_ = true;
	vended_scan_ = nullptr;
}

void BufferingScanTarget::announce(Event event, bool is_visible, const Outputs::Display::ScanTarget::Scan::EndPoint &location, uint8_t composite_amplitude) {
	// Forward the event to the display metrics tracker.
	display_metrics_.announce_event(event);

	if(event == ScanTarget::Event::EndVerticalRetrace) {
		// The previous-frame-is-complete flag is subject to a two-slot queue because
		// measurement for *this* frame needs to begin now, meaning that the previous
		// result needs to be put somewhere — it'll be attached to the first successful
		// line output.
		is_first_in_frame_ = true;
		previous_frame_was_complete_ = frame_is_complete_;
		frame_is_complete_ = true;
	}

	if(output_is_visible_ == is_visible) return;
	if(is_visible) {
		const auto read_pointers = read_pointers_.load();
		std::lock_guard lock_guard(write_pointers_mutex_);

		// Commit the most recent line only if any scans fell on it.
		// Otherwise there's no point outputting it, it'll contribute nothing.
		if(provided_scans_) {
			// Store metadata if concluding a previous line.
			if(active_line_) {
				line_metadata_buffer_[size_t(write_pointers_.line)].is_first_in_frame = is_first_in_frame_;
				line_metadata_buffer_[size_t(write_pointers_.line)].previous_frame_was_complete = previous_frame_was_complete_;
				is_first_in_frame_ = false;
			}

			// Attempt to allocate a new line; note allocation failure if necessary.
			const auto next_line = uint16_t((write_pointers_.line + 1) % LineBufferHeight);
			if(next_line == read_pointers.line) {
				allocation_has_failed_ = true;
				active_line_ = nullptr;
			} else {
				write_pointers_.line = next_line;
				active_line_ = &line_buffer_[size_t(write_pointers_.line)];
			}
			provided_scans_ = 0;
		}

		if(active_line_) {
			active_line_->end_points[0].x = location.x;
			active_line_->end_points[0].y = location.y;
			active_line_->end_points[0].cycles_since_end_of_horizontal_retrace = location.cycles_since_end_of_horizontal_retrace;
			active_line_->end_points[0].composite_angle = location.composite_angle;
			active_line_->line = write_pointers_.line;
			active_line_->composite_amplitude = composite_amplitude;
		}
	} else {
		if(active_line_) {
			// A successfully-allocated line is ending.
			active_line_->end_points[1].x = location.x;
			active_line_->end_points[1].y = location.y;
			active_line_->end_points[1].cycles_since_end_of_horizontal_retrace = location.cycles_since_end_of_horizontal_retrace;
			active_line_->end_points[1].composite_angle = location.composite_angle;

#ifdef LOG_LINES
			if(active_line_->composite_amplitude) {
				std::cout << "L: ";
				std::cout << active_line_->end_points[0].composite_angle << "/" << active_line_->end_points[0].cycles_since_end_of_horizontal_retrace << " -> ";
				std::cout << active_line_->end_points[1].composite_angle << "/" << active_line_->end_points[1].cycles_since_end_of_horizontal_retrace << " => ";
				std::cout << (active_line_->end_points[1].composite_angle - active_line_->end_points[0].composite_angle) << "/" << (active_line_->end_points[1].cycles_since_end_of_horizontal_retrace - active_line_->end_points[0].cycles_since_end_of_horizontal_retrace) << " => ";
				std::cout << double(active_line_->end_points[1].composite_angle - active_line_->end_points[0].composite_angle) / (double(active_line_->end_points[1].cycles_since_end_of_horizontal_retrace - active_line_->end_points[0].cycles_since_end_of_horizontal_retrace) * 64.0f);
				std::cout << std::endl;
			}
#endif
		}

		// A line is complete; submit latest updates if nothing failed.
		if(allocation_has_failed_) {
			// Reset all pointers to where they were; this also means
			// the stencil won't be properly populated.
			write_pointers_ = submit_pointers_.load();
			frame_is_complete_ = false;
		} else {
			// Advance submit pointer.
			submit_pointers_.store(write_pointers_);
		}
		allocation_has_failed_ = false;
	}
	output_is_visible_ = is_visible;
}

const Outputs::Display::Metrics &BufferingScanTarget::display_metrics() {
	return display_metrics_;
}

Outputs::Display::ScanTarget::Scan *BufferingScanTarget::begin_scan() {
	if(allocation_has_failed_) return nullptr;

	std::lock_guard lock_guard(write_pointers_mutex_);

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
	++provided_scans_;

	// Fill in extra OpenGL-specific details.
	result->line = write_pointers_.line;

	vended_scan_ = result;
	return &result->scan;
}

void BufferingScanTarget::set_write_area(uint8_t *base) {
	std::lock_guard lock_guard(write_pointers_mutex_);
	write_area_ = base;
	data_type_size_ = Outputs::Display::size_for_data_type(modals_.input_data_type);
	write_pointers_ = submit_pointers_ = read_pointers_ = PointerSet();
}

size_t BufferingScanTarget::write_area_data_size() const {
	return data_type_size_;
}
