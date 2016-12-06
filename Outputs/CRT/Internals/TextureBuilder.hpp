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
*/
class TextureBuilder {
	public:
		/// Constructs an instance of InputTextureBuilder that contains a texture of colour depth @c bytes_per_pixel;
		/// this creates a new texture and binds it to the current active texture unit.
		TextureBuilder(size_t bytes_per_pixel, GLenum texture_unit);
		virtual ~TextureBuilder();

		/// Finds the first available space of at least @c required_length pixels in size. Calls must be paired off
		/// with calls to @c reduce_previous_allocation_to.
		/// @returns a pointer to the allocated space if any was available; @c nullptr otherwise.
		uint8_t *allocate_write_area(size_t required_length);

		/// Announces that the owner is finished with the region created by the most recent @c allocate_write_area
		/// and indicates that its actual final size was @c actual_length.
		void reduce_previous_allocation_to(size_t actual_length);

		/// @returns @c true if all future calls to @c allocate_write_area will fail on account of the input texture
		/// being full; @c false if calls may succeed.
		bool is_full();

		/// Updates the currently-bound texture with all new data provided since the last @c submit.
		void submit();

		struct WriteArea {
			uint16_t x, y, length;
		};
		void flush(const std::function<void(const std::vector<WriteArea> &write_areas, size_t count)> &);

	private:
		// the buffer size
		size_t bytes_per_pixel_;

		// the buffer
		std::vector<uint8_t> image_;
		GLuint texture_name_;

		// the current list of write areas
		std::vector<WriteArea> write_areas_;
		size_t number_of_write_areas_;
		bool is_full_;
		bool did_submit_;
		inline uint8_t *pointer_to_location(uint16_t x, uint16_t y);

		// Usually: the start position for the current batch of write areas.
		// Caveat: reset to the origin upon a submit. So used in comparison by flush to
		// determine whether the current batch of write areas needs to be relocated.
		uint16_t write_areas_start_x_, write_areas_start_y_;
};

}
}

#endif /* Outputs_CRT_Internals_TextureBuilder_hpp */
