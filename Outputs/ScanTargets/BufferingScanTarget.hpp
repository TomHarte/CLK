//
//  BufferingScanTarget.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 22/07/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#ifndef BufferingScanTarget_hpp
#define BufferingScanTarget_hpp

#include "../ScanTarget.hpp"
#include "../DisplayMetrics.hpp"

#include <array>
#include <atomic>
#include <functional>
#include <mutex>
#include <vector>

namespace Outputs {
namespace Display {

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
			/// Use this plus the scan's endpoints' data_offsets to locate this data in 2d.
			/// Note that the data_offsets will have been adjusted to be relative to the line
			/// they fall within, not the data allocation.
			uint16_t data_y;
			/// Stores the y coordinate assigned to this scan within the intermediate buffers.
			/// Use this plus this scan's endpoints' x locations to determine where to composite
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
				int16_t composite_angle;
			} end_points[2];

			uint16_t line;
			uint8_t composite_amplitude;
		};

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
		};

		/// Sets the area of memory to use as a scan buffer.
		void set_scan_buffer(Scan *buffer, size_t size);

		/// Sets the area of memory to use as line and line metadata buffers.
		void set_line_buffer(Line *line_buffer, LineMetadata *metadata_buffer, size_t size);

		/// Sets a new base address for the texture.
		/// When called this will flush all existing data and load up the
		/// new data size.
		void set_write_area(uint8_t *base);

		/// @returns The number of bytes per input sample, as per the latest modals.
		size_t write_area_data_size() const;

		/// Defines a segment of data now ready for output, consisting of start and endpoints for:
		///
		///	(i) the region of the write area that has been modified; if the caller is using shared memory
		/// for the write area then it can ignore this information;
		///
		/// (ii) the number of scans that have been completed; and
		///
		/// (iii) the number of lines that have been completed.
		///
		/// New write areas and scans are exposed only upon completion of the corresponding lines.
		struct OutputArea {
			struct Endpoint {
				int write_area_x, write_area_y;
				size_t scan;
				size_t line;
			};

			Endpoint start, end;
		};
		/// Performs @c action ensuring that no other @c perform actions, or any
		/// change to modals, occurs simultaneously.
		void perform(const std::function<void(void)> &action);

		/// Acts as per void(void) @c perform but also dequeues all latest available video output.
		void perform(const std::function<void(const OutputArea &)> &);

		/// @returns new Modals if any have been set since the last call to get_new_modals().
		///		The caller must be within a @c perform block.
		const Modals *new_modals();

		/// @returns the current @c Modals.
		const Modals &modals() const;

	private:
		// ScanTarget overrides.
		void set_modals(Modals) final;
		Outputs::Display::ScanTarget::Scan *begin_scan() final;
		void end_scan() final;
		uint8_t *begin_data(size_t required_length, size_t required_alignment) final;
		void end_data(size_t actual_length) final;
		void announce(Event event, bool is_visible, const Outputs::Display::ScanTarget::Scan::EndPoint &location, uint8_t colour_burst_amplitude) final;
		void will_change_owner() final;

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

		// By convention everything in the PointerSet points to the next instance
		// of whatever it is that will be used. So a client should start with whatever
		// is pointed to by the read pointers and carry until it gets to a value that
		// is equal to whatever is in the submit pointers.
		struct PointerSet {
			// This constructor is here to appease GCC's interpretation of
			// an ambiguity in the C++ standard; cf. https://stackoverflow.com/questions/17430377
			PointerSet() noexcept {}

			// Squeezing this struct into 64 bits makes the std::atomics more likely
			// to be lock free; they are under LLVM x86-64.

			// Points to the vended area in the write area texture.
			// The vended area is always preceded by a guard pixel, so a
			// sensible default construction is write_area = 1.
			int32_t write_area = 1;

			// Points into the scan buffer.
			uint16_t scan_buffer = 0;

			// Points into the line buffer.
			uint16_t line = 0;
		};

		/// A pointer to the final thing currently cleared for submission.
		std::atomic<PointerSet> submit_pointers_;

		/// A pointer to the first thing not yet submitted for display; this is
		/// atomic since it also acts as the buffer into which the write_pointers_
		/// may run and is therefore used by both producer and consumer.
		std::atomic<PointerSet> read_pointers_;

		/// This is used as a spinlock to guard `perform` calls.
		std::atomic_flag is_updating_;

		/// A mutex for gettng access to anything the producer modifies — i.e. the write_pointers_,
		/// data_type_size_ and write_area_texture_, and all other state to do with capturing
		/// data, scans and lines.
		///
		/// This is almost never contended. The main collision is a user-prompted change of modals while the
		/// emulation thread is running.
		std::mutex producer_mutex_;

		/// A pointer to the next thing that should be provided to the caller for data.
		PointerSet write_pointers_;

		// The owner-supplied scan buffer and size.
		Scan *scan_buffer_ = nullptr;
		size_t scan_buffer_size_ = 0;

		// The owner-supplied line buffer and size.
		Line *line_buffer_ = nullptr;
		LineMetadata *line_metadata_buffer_ = nullptr;
		size_t line_buffer_size_ = 0;

		// Current modals and whether they've yet been returned
		// from a call to @c get_new_modals.
		Modals modals_;
		bool modals_are_dirty_ = false;
};


}
}

#endif /* BufferingScanTarget_hpp */
