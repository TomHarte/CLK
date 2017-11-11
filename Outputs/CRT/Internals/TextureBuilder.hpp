//
//  TextureBuilder.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 08/03/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Outputs_CRT_Internals_TextureBuilder_hpp
#define Outputs_CRT_Internals_TextureBuilder_hpp

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#include "OpenGL.hpp"
#include "CRTConstants.hpp"

namespace Outputs {
namespace CRT {

/*!
	Owns an OpenGL texture resource and provides mechanisms to fill it from bottom left to top right
	with runs of data, ensuring each run is neighboured immediately to the left and right by copies of its
	first and last pixels.

	Although this class is not itself inherently thread safe, it is built to permit one serialised stream
	of calls to provide source data, with an interceding (but also serialised) submission to the GPU at any time.


	Intended usage by the data generator:

		(i)		allocate a write area with allocate_write_area, supplying a maximum size.
		(ii)	call reduce_previous_allocation_to to announce the actual size written.

	This will cause you to have added source data to the target texture. You can then either use that data
	or allow it to expire.

		(iii)	call retain_latest to add the most recently written write area to the flush queue.

	The flush queue contains provisional data, that can sit in the CPU's memory space indefinitely. This facility
	is provided because it is expected that a texture will be built alontside some other collection of data —
	that data in the flush queue is expected to become useful in coordination with something else but should
	be retained at least until then.

		(iv)	call flush to move data to the submit queue.

	When you flush, you'll receive a record of the bounds of all newly-flushed areas of source data. That gives
	an opportunity to correlate the data with whatever else it is being tied to. It will continue to sit in
	the CPU's memory space but has now passed beyond any further modification or reporting.


	Intended usage by the GPU owner:

		(i)		call submit to move data to the GPU and free up its CPU-side resources.

	The latest data is now on the GPU, regardless of where the data provider may be in its process — only data
	that has entered the submission queue is uploaded.

*/
class TextureBuilder {
	public:
		/// Constructs an instance of InputTextureBuilder that contains a texture of colour depth @c bytes_per_pixel;
		/// this creates a new texture and binds it to the current active texture unit.
		TextureBuilder(size_t bytes_per_pixel, GLenum texture_unit);
		virtual ~TextureBuilder();

		/// Finds the first available space of at least @c required_length pixels in size which is suitably aligned
		/// for writing of @c required_alignment number of pixels at a time.
		/// Calls must be paired off with calls to @c reduce_previous_allocation_to.
		/// @returns a pointer to the allocated space if any was available; @c nullptr otherwise.
		uint8_t *allocate_write_area(size_t required_length, size_t required_alignment = 1);

		/// Announces that the owner is finished with the region created by the most recent @c allocate_write_area
		/// and indicates that its actual final size was @c actual_length.
		void reduce_previous_allocation_to(size_t actual_length);

		/// Allocated runs are provisional; they will not appear in the next flush queue unless retained.
		/// @returns @c true if a retain succeeded; @c false otherwise.
		bool retain_latest();

		// Undoes the most recent retain_latest. Undefined behaviour if a submission has occurred in the interim.
		void discard_latest();

		/// @returns @c true if all future calls to @c allocate_write_area will fail on account of the input texture
		/// being full; @c false if calls may succeed.
		bool is_full();

		/// Updates the currently-bound texture with all new data provided since the last @c submit.
		void submit();

		struct WriteArea {
			uint16_t x, y, length;
		};
		/// Finalises all write areas allocated since the last call to @c flush. Only finalised areas will be
		/// submitted upon the next @c submit. The supplied function will be called with a list of write areas
		/// allocated, indicating their final resting locations and their lengths.
		void flush(const std::function<void(const std::vector<WriteArea> &write_areas, size_t count)> &);

		/// A Bookender helps to paper over precision errors when rendering; its job is to provide single-sample
		/// extensions that duplicate the left and right edges of a written area. By default the texture builder will
		/// simply copy the appropriate number of bytes per pixel, but if the client is using a packed pixel format
		/// then that may be incorrect, e.g. if each sample is a byte but contains two pixels, each in a single nibble,
		/// then the correct duplication might be a byte composed of copies of the two top nibbles as the left bookend,
		/// and one composed of copies of the two bottom nibbles on the right.
		struct Bookender {
			/// Writes to left_bookend the sample that should appear as a continuation before the left_value;
			/// writes to right_bookend the sample that should appear as a continuation after right_value.
			virtual void add_bookends(uint8_t *const left_value, uint8_t *const right_value, uint8_t *left_bookend, uint8_t *right_bookend) = 0;
		};

		/// Sets the current bookender. The bookender be called synchronously within the builder-writing thread.
		/// Supply nullptr to engage the default bookender.
		void set_bookender(std::unique_ptr<Bookender> bookender);

	private:
		// the buffer size
		size_t bytes_per_pixel_;

		// the buffer
		std::vector<uint8_t> image_;
		GLuint texture_name_;

		// the current write area
		WriteArea write_area_;

		// the list of write areas that have ascended to the flush queue
		std::vector<WriteArea> write_areas_;
		size_t number_of_write_areas_ = 0;
		bool is_full_ = false, was_full_ = false;
		uint16_t first_unsubmitted_y_ = 0;
		inline uint8_t *pointer_to_location(uint16_t x, uint16_t y);

		// Usually: the start position for the current batch of write areas.
		// Caveat: reset to the origin upon a submit. So used in comparison by flush to
		// determine whether the current batch of write areas needs to be relocated.
		uint16_t write_areas_start_x_ = 0, write_areas_start_y_ = 0;

		std::unique_ptr<Bookender> bookender_;
};

}
}

#endif /* Outputs_CRT_Internals_TextureBuilder_hpp */
