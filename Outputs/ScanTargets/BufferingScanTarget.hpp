//
//  BufferingScanTarget.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 22/07/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#pragma once

#include "Outputs/ScanTarget.hpp"
#include "Outputs/DisplayMetrics.hpp"
#include "Concurrency/SpinLock.hpp"
#include "Numeric/CircularCounter.hpp"

#include <array>
#include <atomic>
#include <mutex>
#include <vector>

namespace Outputs::Display {

/*!
	Provides basic thread-safe (hopefully) circular queues for any scan target that:

		*	will store incoming Scans into a linear circular buffer and pack regions of
			incoming pixel data into a 2048x2048 2d texture;
		*	will compose whole lines of content by partioning the Scans based on sync
			placement and then pasting together their content;
		*	will process those lines as necessary to map from input format to whatever
			suits the display; and
		*	will then output the lines.

	This buffer rejects new data when full.
*/
class BufferingScanTarget: public Outputs::Display::ScanTarget {
public:
	/*! @returns The DisplayMetrics object that this ScanTarget has been providing with announcements and draw overages. */
	const Metrics &display_metrics();

	static constexpr int WriteAreaWidth = 2048;
	static constexpr int WriteAreaHeight = 2048;

	BufferingScanTarget();

	// This is included because it's assumed that scan targets will want to expose one.
	// It is the subclass's responsibility to post timings.
	Metrics display_metrics_;

	/// Extends the definition of a Scan to include two extra fields,
	/// completing this scan's source data and destination locations.
	struct Scan {
		Outputs::Display::ScanTarget::Scan scan;

		/// Stores the y coordinate for this scan's data within the write area texture.
		/// Use this plus the scan's endpoints' `data_offsets` to locate this data in 2d.
		/// Note that the `data_offsets` will have been adjusted to be relative to the line
		/// they fall within, not the data allocation.
		uint16_t data_y;
		/// Stores the y coordinate assigned to this scan within the intermediate buffers.
		/// Use this plus this scan's endpoint x locations to determine where to compose
		/// this data for intermediate processing.
		uint16_t line;
	};

	/// Defines the boundaries of a complete line of video — a 2d start and end location,
	/// composite phase and amplitude (if relevant), the source line in the intermediate buffer
	/// plus the start and end offsets of the area that is visible from the intermediate buffer.
	struct Line {
		struct EndPoint {
			uint16_t x, y;
			uint16_t cycles_since_end_of_horizontal_retrace;
		} end_points[2];

		uint16_t line;
	};
	static_assert(sizeof(Line) == 14);

	/// Provides additional metadata about lines; this is separate because it's unlikely to be of
	/// interest to the GPU, unlike the fields in Line.
	struct LineMetadata {
		/// @c true if this line was the first drawn after vertical sync; @c false otherwise.
		bool is_first_in_frame;
		/// @c true if this line is the first in the frame and if every single piece of output
		/// from the previous frame was recorded; @c false otherwise. Data can be dropped
		/// from a frame if performance problems mean that the emulated machine is running
		/// more quickly than complete frames can be generated.
		bool previous_frame_was_complete;
		/// The index of the first scan that will appear on this line.
		size_t first_scan;
	};

	/// Sets the area of memory to use as a scan buffer.
	void set_scan_buffer(Scan *buffer, size_t size);

	/// Sets the area of memory to use as line and line metadata buffers.
	void set_line_buffer(Line *line_buffer, size_t size);

	/// Sets a new base address for the texture.
	/// When called this will flush all existing data and load up the
	/// new data size.
	void set_write_area(uint8_t *base);

	/// @returns The number of bytes per input sample, as per the latest modals.
	size_t write_area_data_size() const;

	/// Defines a segment of data now ready for output, consisting of begin and end endpoints for:
	///
	///	(i) the region of the write area that has been modified; if the caller is using shared memory
	/// for the write area then it can ignore this information;
	///
	/// (ii) the number of scans that have been completed; and
	///
	/// (iii) the number of lines that have been completed.
	///
	/// New write areas and scans are exposed only upon completion of the corresponding lines.
	/// The values indicated by the begin point are the first that should be drawn. Those indicated
	/// by the end point are one after the final that should be drawn.
	///
	/// So e.g. begin.scan = 23, end.scan = 24 means draw a single scan, index 23.
	struct OutputArea {
		struct Endpoint {
			int write_area_x, write_area_y;
			size_t scan;
			size_t line;
			size_t frame;
		};

		Endpoint begin, end;

#ifndef NDEBUG
		size_t counter;
#endif
	};

	/// Gets the current range of content that has been posted but not yet returned by
	/// a previous call to get_output_area().
	///
	/// Does not require the caller to be within a @c perform block.
	OutputArea get_output_area();

	/// Announces that the output area has now completed output, freeing up its memory for
	/// further modification.
	///
	/// It is the caller's responsibility to ensure that the areas passed to complete_output_area
	/// are those from get_output_area and are marked as completed in the same order that
	/// they were originally provided.
	///
	/// Does not require the caller to be within a @c perform block.
	void complete_output_area(const OutputArea &);

	/// Performs @c action ensuring that no other @c perform actions, or any
	/// change to modals, occurs simultaneously.
	template <typename FuncT>
	void perform(FuncT &&function) {
		std::lock_guard guard(is_updating_);
		function();
	}

	/// @returns new Modals if any have been set since the last call to get_new_modals().
	///		The caller must be within a @c perform block.
	const Modals *new_modals();

	/// @returns the current @c Modals.
	const Modals &modals() const;

	/// @returns whether the current output being received appears to describe an interlaced signal.
	/// This is a hint only, potentially to provide for better deinterlacing of output, being derived locally
	/// from line positioning. Specifically: if a scan target pays no heed to this whatsoever it's likely to
	/// end up doing the equivalent of a bob. If it so desires, it might prefer to do something closer to
	/// a weave if and only if interlaced video is detected.
	bool is_interlaced() const;

	/// @returns @c true if new modals are available; @c false otherwise.
	///
	/// Safe to call from any thread.
	bool has_new_modals() const;

	template <typename OutputFuncT, typename FrameFuncT, typename CountGetT, typename FrameLimitGetT>
	void output(
		const OutputArea &area,
		const OutputFuncT &&output,
		const FrameFuncT &&end_frame,
		const CountGetT &&count,
		const FrameLimitGetT &&limit
	) const {
		if(count(area.end) == count(area.begin) && area.end.frame == area.begin.frame) {
			return;
		}

		if(area.end.frame == area.begin.frame) {
			output(count(area.begin), count(area.end));
			return;
		}

		size_t output_begin = count(area.begin);
		size_t frame_begin = area.begin.frame;
		do {
			if(output_begin != limit(frames_[frame_begin])) {
				output(output_begin, limit(frames_[frame_begin]));
				output_begin = limit(frames_[frame_begin]);
			}
			end_frame(frames_[frame_begin].was_complete);

			++frame_begin;
			if(frame_begin == frames_.size()) frame_begin = 0;
		} while(frame_begin != area.end.frame);

		if(output_begin != count(area.end)) {
			output(output_begin, count(area.end));
		}
	}

	template <typename ScanFuncT, typename FrameFuncT>
	void output_scans(
		const OutputArea &area,
		const ScanFuncT &&output_scans,
		const FrameFuncT &&end_frame
	) const {
		output(
			area,
			std::move(output_scans),
			std::move(end_frame),
			[](const auto &endpoint) { return endpoint.scan; },
			[](const auto &frame) { return frame.first_scan; }
		);
	}

	template <typename LineFuncT, typename FrameFuncT>
	void output_lines(
		const OutputArea &area,
		const LineFuncT &&output_lines,
		const FrameFuncT &&end_frame
	) const {
		output(
			area,
			std::move(output_lines),
			std::move(end_frame),
			[](const auto &endpoint) { return endpoint.line; },
			[](const auto &frame) { return frame.first_line; }
		);
	}

private:
	// ScanTarget overrides.
	void set_modals(Modals) final;
	Outputs::Display::ScanTarget::Scan *begin_scan() final;
	void end_scan() final;
	uint8_t *begin_data(size_t required_length, size_t required_alignment) final;
	void end_data(size_t actual_length) final;
	void announce(Event event, bool is_visible, const Outputs::Display::ScanTarget::Scan::EndPoint &location, uint8_t colour_burst_amplitude) final;
	void will_change_owner() final;
	void set_delegate(Delegate &) final;

	// Uses a texture to vend write areas.
	uint8_t *write_area_ = nullptr;
	size_t data_type_size_ = 0;

	// Tracks changes in raster visibility in order to populate
	// Lines and LineMetadatas.
	bool output_is_visible_ = false;

	// Track allocation failures.
	bool data_is_allocated_ = false;
	bool allocation_has_failed_ = false;

	// Ephemeral information for the begin/end functions.
	Scan *vended_scan_ = nullptr;
	int vended_write_area_pointer_ = 0;

	// Ephemeral state that helps in line composition.
	int provided_scans_ = 0;
	bool is_first_in_frame_ = true;
	bool frame_is_complete_ = true;
	bool previous_frame_was_complete_ = true;

	Concurrency::SpinLock<Concurrency::Barrier::AcquireRelease> is_updating_;

	/// A lock for gettng access to anything the producer modifies — i.e. the write_pointers_,
	/// data_type_size_ and write_area_texture_, and all other state to do with capturing
	/// data, scans and lines.
	///
	/// This is almost never contended. The main collision is a user-prompted change of modals while the
	/// emulation thread is running.
	Concurrency::SpinLock<Concurrency::Barrier::Relaxed> producer_lock_;

	// The owner-supplied scan buffer and size.
	Scan *scan_buffer_ = nullptr;
	size_t scan_buffer_size_ = 0;

	// The owner-supplied line buffer and size.
	Line *line_buffer_ = nullptr;
	size_t line_buffer_size_ = 0;

	// Current modals and whether they've yet been returned
	// from a call to @c get_new_modals.
	Modals modals_;
	std::atomic<bool> modals_are_dirty_ = false;

	// Provides a per-data size implementation of end_data; a previous
	// implementation used blind memcpy and that turned into something
	// of a profiling hot spot.
	template <typename DataUnit> void end_data(size_t actual_length);

#ifndef NDEBUG
	// Debug features; these amount to API validation.
	bool scan_is_ongoing_ = false;
	size_t output_area_counter_ = 0;
	size_t output_area_next_returned_ = 0;
#endif

	// Frames parts kept separately from the PointerSet as that is subject
	// to back pressure, meaning that data may be dropped.
	//
	// Frames always advance as a simple function of time and have a very
	// large buffer in terms of clock time.
	struct Frame {
		size_t first_scan;
		size_t first_line;
		bool was_complete;
	};
	static constexpr uint16_t NumFrames = 60;
	std::array<Frame, NumFrames> frames_;
	std::atomic<Numeric::CircularCounter<uint16_t, NumFrames>> frame_read_;
	std::atomic<Numeric::CircularCounter<uint16_t, NumFrames>> frame_write_;

	// A recent history of field start positions, to allow detection of interlaced video.
	// That can be used as a hint for display purposes.
	static constexpr size_t StartHistoryLength = 16;
	std::array<Outputs::Display::ScanTarget::Scan::EndPoint, StartHistoryLength> start_history_;
	Numeric::CircularCounter<size_t, StartHistoryLength> start_history_pointer_;
	std::atomic<bool> is_interlaced_ = false;

	// By convention everything in the PointerSet points to the next instance
	// of whatever it is that will be used. So a client should start with whatever
	// is pointed to by the read pointers and carry until it gets to a value that
	// is equal to whatever is in the submit pointers.
	struct PointerSet {
		// This constructor is here to appease GCC's interpretation of
		// an ambiguity in the C++ standard; cf. https://stackoverflow.com/questions/17430377
		PointerSet() noexcept = default;

		// Squeezing this struct into 64 bits ensures the std::atomics are lock free
		// on the platforms and compilers I target.

		// Points to the vended area in the write area texture.
		// The vended area is always preceded by a guard pixel, so a
		// sensible default construction is write_area = 1.
		int32_t write_area = 1;

		// Points into the scan buffer.
		uint16_t scan = 0;

		// Points into the line buffer.
		uint16_t line = 0;
	};
	static_assert(std::atomic<PointerSet>::is_always_lock_free);

	/// A pointer to the next thing that should be provided to the caller for data.
	PointerSet write_pointers_;

	/// A pointer to the final thing currently cleared for submission.
	alignas(64) std::atomic<PointerSet> submit_pointers_;

	/// A pointer to the first thing not yet submitted for display; this is
	/// atomic since it also acts as the buffer into which the write_pointers_
	/// may run and is therefore used by both producer and consumer.
	alignas(64) std::atomic<PointerSet> read_pointers_;

	alignas(64) std::atomic<PointerSet> read_ahead_pointers_;
};

}
