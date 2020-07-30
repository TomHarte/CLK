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

// If enabled, this uses the producer lock to cover both production and consumption
// rather than attempting to proceed lockfree. This is primarily for diagnostic purposes;
// it allows empirical exploration of whether the logical and memory barriers that are
// meant to mediate things between the read pointers and the submit pointers are functioning.
#define ONE_BIG_LOCK

#define TextureAddressGetY(v)	uint16_t((v) >> 11)
#define TextureAddressGetX(v)	uint16_t((v) & 0x7ff)
#define TextureSub(a, b)		(((a) - (b)) & 0x3fffff)
#define TextureAddress(x, y)	(((y) << 11) | (x))

using namespace Outputs::Display;

BufferingScanTarget::BufferingScanTarget() {
	// Ensure proper initialisation of the two atomic pointer sets.
	read_pointers_.store(write_pointers_, std::memory_order::memory_order_relaxed);
	submit_pointers_.store(write_pointers_, std::memory_order::memory_order_relaxed);

	// Establish initial state for is_updating_.
	is_updating_.clear(std::memory_order::memory_order_relaxed);
}

// MARK: - Producer; pixel data.

uint8_t *BufferingScanTarget::begin_data(size_t required_length, size_t required_alignment) {
	assert(required_alignment);

	// Acquire the standard producer lock, nominally over write_pointers_.
	std::lock_guard lock_guard(producer_mutex_);

	// If allocation has already failed on this line, continue the trend.
	if(allocation_has_failed_) return nullptr;

	// If there isn't yet a write area then mark allocation as failed and finish.
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
	const auto read_pointers = read_pointers_.load(std::memory_order::memory_order_relaxed);

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
	// Acquire the producer lock.
	std::lock_guard lock_guard(producer_mutex_);

	// Do nothing if no data write is actually ongoing.
	if(allocation_has_failed_ || !data_is_allocated_) return;

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

// MARK: - Producer; scans.

Outputs::Display::ScanTarget::Scan *BufferingScanTarget::begin_scan() {
	std::lock_guard lock_guard(producer_mutex_);

	// If there's already an allocation failure on this line, do no work.
	if(allocation_has_failed_) {
		vended_scan_ = nullptr;
		return nullptr;
	}

	const auto result = &scan_buffer_[write_pointers_.scan_buffer];
	const auto read_pointers = read_pointers_.load(std::memory_order::memory_order_relaxed);

	// Advance the pointer.
	const auto next_write_pointer = decltype(write_pointers_.scan_buffer)((write_pointers_.scan_buffer + 1) % scan_buffer_size_);

	// Check whether that's too many.
	if(next_write_pointer == read_pointers.scan_buffer) {
		allocation_has_failed_ = true;
		vended_scan_ = nullptr;
		return nullptr;
	}
	write_pointers_.scan_buffer = next_write_pointer;
	++provided_scans_;

	// Fill in extra OpenGL-specific details.
	result->line = write_pointers_.line;

	vended_scan_ = result;
	return &result->scan;
}

void BufferingScanTarget::end_scan() {
	std::lock_guard lock_guard(producer_mutex_);

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
		// line output.
		is_first_in_frame_ = true;
		previous_frame_was_complete_ = frame_is_complete_;
		frame_is_complete_ = true;
	}

	// Proceed from here only if a change in visibility has occurred.
	if(output_is_visible_ == is_visible) return;
	output_is_visible_ = is_visible;

	if(is_visible) {
		const auto read_pointers = read_pointers_.load(std::memory_order::memory_order_relaxed);

		// Attempt to allocate a new line, noting allocation failure if necessary.
		const auto next_line = uint16_t((write_pointers_.line + 1) % line_buffer_size_);
		if(next_line == read_pointers.line) {
			allocation_has_failed_ = true;
		}
		provided_scans_ = 0;

		// If there was space for a new line, establish its start.
		if(!allocation_has_failed_) {
			Line &active_line = line_buffer_[size_t(write_pointers_.line)];
			active_line.end_points[0].x = location.x;
			active_line.end_points[0].y = location.y;
			active_line.end_points[0].cycles_since_end_of_horizontal_retrace = location.cycles_since_end_of_horizontal_retrace;
			active_line.end_points[0].composite_angle = location.composite_angle;
			active_line.line = write_pointers_.line;
			active_line.composite_amplitude = composite_amplitude;
		}
	} else {
		// Commit the most recent line only if any scans fell on it and all allocation was successful.
		if(!allocation_has_failed_ && provided_scans_) {
			// Store metadata.
			LineMetadata &metadata = line_metadata_buffer_[size_t(write_pointers_.line)];
			metadata.is_first_in_frame = is_first_in_frame_;
			metadata.previous_frame_was_complete = previous_frame_was_complete_;
			is_first_in_frame_ = false;

			// Store actual line data.
			Line &active_line = line_buffer_[size_t(write_pointers_.line)];
			active_line.end_points[1].x = location.x;
			active_line.end_points[1].y = location.y;
			active_line.end_points[1].cycles_since_end_of_horizontal_retrace = location.cycles_since_end_of_horizontal_retrace;
			active_line.end_points[1].composite_angle = location.composite_angle;

			// Advance the line pointer.
			write_pointers_.line = uint16_t((write_pointers_.line + 1) % line_buffer_size_);

			// Update the submit pointers with all lines, scans and data written during this line.
			submit_pointers_.store(write_pointers_, std::memory_order::memory_order_release);
		} else {
			// Something failed, or there was nothing on the line anyway, so reset all pointers to where they
			// were before this line. Mark frame as incomplete if this was an allocation failure.
			write_pointers_ = submit_pointers_.load(std::memory_order::memory_order_relaxed);
			frame_is_complete_ &= !allocation_has_failed_;
		}

		// Reset the allocation-has-failed flag for the next line
		// and mark no line as active.
		allocation_has_failed_ = false;
	}
}

// MARK: - Producer; other state.

void BufferingScanTarget::will_change_owner() {
	std::lock_guard lock_guard(producer_mutex_);
	allocation_has_failed_ = true;
	vended_scan_ = nullptr;
}

const Outputs::Display::Metrics &BufferingScanTarget::display_metrics() {
	return display_metrics_;
}

void BufferingScanTarget::set_write_area(uint8_t *base) {
	// This is a bit of a hack. This call needs the producer mutex and should be
	// safe to call from a @c perform block in order to support all potential consumers.
	// But the temporary hack of ONE_BIG_LOCK then implies that either I need a recursive
	// mutex, or I have to make a coupling assumption about my caller. I've done the latter,
	// because ONE_BIG_LOCK is really really meant to be temporary. I hope.
#ifndef ONE_BIG_LOCK
	std::lock_guard lock_guard(producer_mutex_);
#endif
	write_area_ = base;
	data_type_size_ = Outputs::Display::size_for_data_type(modals_.input_data_type);
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
	perform([=] {
		modals_ = modals;
		modals_are_dirty_ = true;
	});
}

// MARK: - Consumer.

void BufferingScanTarget::perform(const std::function<void(const OutputArea &)> &function) {
#ifdef ONE_BIG_LOCK
	std::lock_guard lock_guard(producer_mutex_);
#endif

	// The area to draw is that between the read pointers, representing wherever reading
	// last stopped, and the submit pointers, representing all the new data that has been
	// cleared for submission.
	const auto submit_pointers = submit_pointers_.load(std::memory_order::memory_order_acquire);
	const auto read_pointers = read_pointers_.load(std::memory_order::memory_order_relaxed);

	OutputArea area;

	area.start.line = read_pointers.line;
	area.end.line = submit_pointers.line;

	area.start.scan = read_pointers.scan_buffer;
	area.end.scan = submit_pointers.scan_buffer;

	area.start.write_area_x = TextureAddressGetX(read_pointers.write_area);
	area.start.write_area_y = TextureAddressGetY(read_pointers.write_area);
	area.end.write_area_x = TextureAddressGetX(submit_pointers.write_area);
	area.end.write_area_y = TextureAddressGetY(submit_pointers.write_area);

	// Perform only while holding the is_updating lock.
	while(is_updating_.test_and_set(std::memory_order_acquire));
	function(area);
	is_updating_.clear(std::memory_order_release);

	// Update the read pointers.
	read_pointers_.store(submit_pointers, std::memory_order::memory_order_relaxed);
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
	if(!modals_are_dirty_) {
		return nullptr;
	}
	modals_are_dirty_ = false;
	return &modals_;
}

const Outputs::Display::ScanTarget::Modals &BufferingScanTarget::modals() const {
	return modals_;
}
