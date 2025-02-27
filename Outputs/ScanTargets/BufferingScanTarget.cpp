//
//  BufferingScanTarget.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 22/07/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#include "BufferingScanTarget.hpp"

#include <cassert>
#include <cstring>

#define TextureAddressGetY(v)	uint16_t((v) >> 11)
#define TextureAddressGetX(v)	uint16_t((v) & 0x7ff)
#define TextureSub(a, b)		(((a) - (b)) & 0x3fffff)
#define TextureAddress(x, y)	(((y) << 11) | (x))

using namespace Outputs::Display;

BufferingScanTarget::BufferingScanTarget() {
	// Ensure proper initialisation of the two atomic pointer sets.
	read_pointers_.store(write_pointers_, std::memory_order_relaxed);
	submit_pointers_.store(write_pointers_, std::memory_order_relaxed);

	// Establish initial state for is_updating_.
	is_updating_.clear(std::memory_order_relaxed);
}

// MARK: - Producer; pixel data.

uint8_t *BufferingScanTarget::begin_data(size_t required_length, size_t required_alignment) {
	assert(required_alignment);

	// Acquire the standard producer lock, nominally over write_pointers_.
	std::lock_guard lock_guard(producer_mutex_);

	// If allocation has already failed on this line, continue the trend.
	if(allocation_has_failed_) return nullptr;

	// If there isn't yet a write area or data size then mark allocation as failed and finish.
	if(!write_area_ || !data_type_size_) {
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

	// Check whether that steps over the read pointer; if so then the final address will be closer
	// to the write pointer than the old.
	const auto end_address = TextureAddress(end_x, output_y);
	const auto read_pointers = read_pointers_.load(std::memory_order_relaxed);

	const auto end_distance = TextureSub(end_address, read_pointers.write_area);
	const auto previous_distance = TextureSub(write_pointers_.write_area, read_pointers.write_area);

	// Perform a quick sanity check.
	assert(end_distance >= 0);
	assert(previous_distance >= 0);

	// If allocating this would somehow make the write pointer back away from the read pointer,
	// there must not be enough space left.
	if(end_distance < previous_distance) {
		allocation_has_failed_ = true;
		return nullptr;
	}

	// Everything checks out, note expectation of a future end_data and return the pointer.
	assert(!data_is_allocated_);
	data_is_allocated_ = true;
	vended_write_area_pointer_ = write_pointers_.write_area = TextureAddress(aligned_start_x, output_y);

	assert(write_pointers_.write_area >= 1 && ((size_t(write_pointers_.write_area) + required_length + 1) * data_type_size_) <= WriteAreaWidth*WriteAreaHeight*data_type_size_);
	return &write_area_[size_t(write_pointers_.write_area) * data_type_size_];

	// Note state at exit:
	//		write_pointers_.write_area points to the first pixel the client is expected to draw to.
}

template <typename DataUnit> void BufferingScanTarget::end_data(size_t actual_length) {
	// Bookend the start and end of the new data, to safeguard for precision errors in sampling.
	DataUnit *const sized_write_area = &reinterpret_cast<DataUnit *>(write_area_)[write_pointers_.write_area];
	sized_write_area[-1] = sized_write_area[0];
	sized_write_area[actual_length] = sized_write_area[actual_length - 1];
}

void BufferingScanTarget::end_data(size_t actual_length) {
	// Acquire the producer lock.
	std::lock_guard lock_guard(producer_mutex_);

	// Do nothing if no data write is actually ongoing.
	if(!data_is_allocated_) return;
	data_is_allocated_ = false;

	// Check for other allocation failures.
	if(allocation_has_failed_) return;

	// Apply necessary bookends.
	switch(data_type_size_) {
		default: assert(false);
		case 0:
			// This just means that modals haven't been grabbed yet. So it's not
			// a valid data type size, but it is a value that might legitimately
			// be seen here.
		break;
		case 1:	end_data<uint8_t>(actual_length);	break;
		case 2:	end_data<uint16_t>(actual_length);	break;
		case 4:	end_data<uint32_t>(actual_length);	break;
	}

	// Advance to the end of the current run.
	write_pointers_.write_area += actual_length + 1;

	// The write area was allocated in the knowledge that there's sufficient
	// distance left on the current line, but there's a risk of exactly filling
	// the final line, in which case this should wrap back to 0.
	write_pointers_.write_area %= WriteAreaWidth*WriteAreaHeight;
}

// MARK: - Producer; scans.

Outputs::Display::ScanTarget::Scan *BufferingScanTarget::begin_scan() {
	std::lock_guard lock_guard(producer_mutex_);

	// If there's already an allocation failure on this line, do no work.
	if(allocation_has_failed_) {
		vended_scan_ = nullptr;
		return nullptr;
	}

	const auto result = &scan_buffer_[write_pointers_.scan];
	const auto read_pointers = read_pointers_.load(std::memory_order_relaxed);

	// Advance the pointer.
	const auto next_write_pointer = decltype(write_pointers_.scan)((write_pointers_.scan + 1) % scan_buffer_size_);

	// Check whether that's too many.
	if(next_write_pointer == read_pointers.scan) {
		allocation_has_failed_ = true;
		vended_scan_ = nullptr;
		return nullptr;
	}
	write_pointers_.scan = next_write_pointer;
	++provided_scans_;

	// Fill in extra OpenGL-specific details.
	result->line = write_pointers_.line;

	vended_scan_ = result;

#ifndef NDEBUG
	assert(!scan_is_ongoing_);
	scan_is_ongoing_ = true;
#endif

	return &result->scan;
}

void BufferingScanTarget::end_scan() {
	std::lock_guard lock_guard(producer_mutex_);

#ifndef NDEBUG
	assert(scan_is_ongoing_);
	scan_is_ongoing_ = false;
#endif

	// Complete the scan only if one is afoot.
	if(vended_scan_) {
		vended_scan_->data_y = TextureAddressGetY(vended_write_area_pointer_);
		vended_scan_->line = write_pointers_.line;
		vended_scan_->scan.end_points[0].data_offset += TextureAddressGetX(vended_write_area_pointer_);
		vended_scan_->scan.end_points[1].data_offset += TextureAddressGetX(vended_write_area_pointer_);
		vended_scan_ = nullptr;
	}
}

// MARK: - Producer; lines.

void BufferingScanTarget::announce(Event event, bool is_visible, const Outputs::Display::ScanTarget::Scan::EndPoint &location, uint8_t composite_amplitude) {
	std::lock_guard lock_guard(producer_mutex_);

	// Forward the event to the display metrics tracker.
	display_metrics_.announce_event(event);

	if(event == ScanTarget::Event::EndVerticalRetrace) {
		// The previous-frame-is-complete flag is subject to a two-slot queue because
		// measurement for *this* frame needs to begin now, meaning that the previous
		// result needs to be put somewhere — it'll be attached to the first successful
		// line output, whenever that comes.
		is_first_in_frame_ = true;
		previous_frame_was_complete_ = frame_is_complete_;
		frame_is_complete_ = true;
	}

	// Proceed from here only if a change in visibility has occurred.
	if(output_is_visible_ == is_visible) return;
	output_is_visible_ = is_visible;

#ifndef NDEBUG
	assert(!scan_is_ongoing_);
#endif

	if(is_visible) {
		const auto read_pointers = read_pointers_.load(std::memory_order_relaxed);

		// Attempt to allocate a new line, noting allocation success or failure.
		const auto next_line = uint16_t((write_pointers_.line + 1) % line_buffer_size_);
		allocation_has_failed_ = next_line == read_pointers.line;
		if(!allocation_has_failed_) {
			// If there was space for a new line, establish its start and reset the count of provided scans.
			Line &active_line = line_buffer_[size_t(write_pointers_.line)];
			active_line.end_points[0].x = location.x;
			active_line.end_points[0].y = location.y;
			active_line.end_points[0].cycles_since_end_of_horizontal_retrace = location.cycles_since_end_of_horizontal_retrace;
			active_line.end_points[0].composite_angle = location.composite_angle;
			active_line.line = write_pointers_.line;
			active_line.composite_amplitude = composite_amplitude;

			provided_scans_ = 0;
		}
	} else {
		// Commit the most recent line only if any scans fell on it and all allocation was successful.
		if(!allocation_has_failed_ && provided_scans_) {
			const auto submit_pointers = submit_pointers_.load(std::memory_order_relaxed);

			// Store metadata.
			LineMetadata &metadata = line_metadata_buffer_[size_t(write_pointers_.line)];
			metadata.is_first_in_frame = is_first_in_frame_;
			metadata.previous_frame_was_complete = previous_frame_was_complete_;
			metadata.first_scan = submit_pointers.scan;
			is_first_in_frame_ = false;

			// Sanity check.
			assert(((metadata.first_scan + size_t(provided_scans_)) % scan_buffer_size_) == write_pointers_.scan);

			// Store actual line data.
			Line &active_line = line_buffer_[size_t(write_pointers_.line)];
			active_line.end_points[1].x = location.x;
			active_line.end_points[1].y = location.y;
			active_line.end_points[1].cycles_since_end_of_horizontal_retrace = location.cycles_since_end_of_horizontal_retrace;
			active_line.end_points[1].composite_angle = location.composite_angle;

			// Advance the line pointer.
			write_pointers_.line = uint16_t((write_pointers_.line + 1) % line_buffer_size_);

			// Update the submit pointers with all lines, scans and data written during this line.
			std::atomic_thread_fence(std::memory_order_release);
			submit_pointers_.store(write_pointers_, std::memory_order_release);
		} else {
			// Something failed, or there was nothing on the line anyway, so reset all pointers to where they
			// were before this line. Mark frame as incomplete if this was an allocation failure.
			write_pointers_ = submit_pointers_.load(std::memory_order_relaxed);
			frame_is_complete_ &= !allocation_has_failed_;
		}
	}
}

// MARK: - Producer; other state.

void BufferingScanTarget::will_change_owner() {
	std::lock_guard lock_guard(producer_mutex_);
	allocation_has_failed_ = true;
	vended_scan_ = nullptr;
#ifndef NDEBUG
	data_is_allocated_ = false;
#endif
}

const Outputs::Display::Metrics &BufferingScanTarget::display_metrics() {
	return display_metrics_;
}

void BufferingScanTarget::set_write_area(uint8_t *base) {
	std::lock_guard lock_guard(producer_mutex_);
	write_area_ = base;
	write_pointers_ = submit_pointers_ = read_pointers_ = PointerSet();
	allocation_has_failed_ = true;
	vended_scan_ = nullptr;
}

size_t BufferingScanTarget::write_area_data_size() const {
	// TODO: can I guarantee this is safe without requiring that set_write_area
	// be within an @c perform block?
	return data_type_size_;
}

void BufferingScanTarget::set_modals(Modals modals) {
	perform([&] {
		modals_ = modals;
		modals_are_dirty_.store(true, std::memory_order_relaxed);
	});
}

// MARK: - Consumer.

BufferingScanTarget::OutputArea BufferingScanTarget::get_output_area() {
	// The area to draw is that between the read pointers, representing wherever reading
	// last stopped, and the submit pointers, representing all the new data that has been
	// cleared for submission.
	const auto submit_pointers = submit_pointers_.load(std::memory_order_acquire);
	const auto read_ahead_pointers = read_ahead_pointers_.load(std::memory_order_relaxed);
	std::atomic_thread_fence(std::memory_order_acquire);

	OutputArea area;

	area.start.line = read_ahead_pointers.line;
	area.end.line = submit_pointers.line;

	area.start.scan = read_ahead_pointers.scan;
	area.end.scan = submit_pointers.scan;

	area.start.write_area_x = TextureAddressGetX(read_ahead_pointers.write_area);
	area.start.write_area_y = TextureAddressGetY(read_ahead_pointers.write_area);
	area.end.write_area_x = TextureAddressGetX(submit_pointers.write_area);
	area.end.write_area_y = TextureAddressGetY(submit_pointers.write_area);

	// Update the read-ahead pointers.
	read_ahead_pointers_.store(submit_pointers, std::memory_order_relaxed);

#ifndef NDEBUG
	area.counter = output_area_counter_;
	++output_area_counter_;
#endif

	return area;
}

void BufferingScanTarget::complete_output_area(const OutputArea &area) {
	// TODO: check that this is the expected next area if in DEBUG mode.

	PointerSet new_read_pointers;
	new_read_pointers.line = uint16_t(area.end.line);
	new_read_pointers.scan = uint16_t(area.end.scan);
	new_read_pointers.write_area = TextureAddress(area.end.write_area_x, area.end.write_area_y);
	read_pointers_.store(new_read_pointers, std::memory_order_relaxed);

#ifndef NDEBUG
	// This will fire if the caller is announcing completed output areas out of order.
	assert(area.counter == output_area_next_returned_);
	++output_area_next_returned_;
#endif
}

void BufferingScanTarget::perform(const std::function<void(void)> &function) {
	while(is_updating_.test_and_set(std::memory_order_acquire));
	function();
	is_updating_.clear(std::memory_order_release);
}

void BufferingScanTarget::set_scan_buffer(Scan *buffer, size_t size) {
	scan_buffer_ = buffer;
	scan_buffer_size_ = size;
}

void BufferingScanTarget::set_line_buffer(Line *line_buffer, LineMetadata *metadata_buffer, size_t size) {
	line_buffer_ = line_buffer;
	line_metadata_buffer_ = metadata_buffer;
	line_buffer_size_ = size;
}

const Outputs::Display::ScanTarget::Modals *BufferingScanTarget::new_modals() {
	const auto modals_are_dirty = modals_are_dirty_.load(std::memory_order_relaxed);
	if(!modals_are_dirty) {
		return nullptr;
	}

	modals_are_dirty_.store(false, std::memory_order_relaxed);

	// MAJOR SHARP EDGE HERE: assume that because the new_modals have been fetched then the caller will
	// now ensure their texture buffer is appropriate. They might provide a new pointer and might now.
	// But either way it's now appropriate to start treating the data size as implied by the data type.
	std::lock_guard lock_guard(producer_mutex_);
	data_type_size_ = Outputs::Display::size_for_data_type(modals_.input_data_type);
	assert((data_type_size_ == 1) || (data_type_size_ == 2) || (data_type_size_ == 4));

	return &modals_;
}

const Outputs::Display::ScanTarget::Modals &BufferingScanTarget::modals() const {
	return modals_;
}

bool BufferingScanTarget::has_new_modals() const {
	return modals_are_dirty_.load(std::memory_order_relaxed);
}
