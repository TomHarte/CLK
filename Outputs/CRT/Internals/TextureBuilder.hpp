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
		TextureBuilder(size_t bytes_per_pixel);
		virtual ~TextureBuilder();

		/// Finds the first available space of at least @c required_length pixels in size. Calls must be paired off
		/// with calls to @c reduce_previous_allocation_to.
		/// @returns a pointer to the allocated space if any was available; @c nullptr otherwise.
		uint8_t *allocate_write_area(size_t required_length);

		/// Announces that the owner is finished with the region created by the most recent @c allocate_write_area
		/// and indicates that its actual final size was @c actual_length.
		void reduce_previous_allocation_to(size_t actual_length);

		/// @returns the start column for the most recent allocated write area.
		uint16_t get_last_write_x_position();

		/// @returns the row of the most recent allocated write area.
		uint16_t get_last_write_y_position();

		/// @returns @c true if all future calls to @c allocate_write_area will fail on account of the input texture
		/// being full; @c false if calls may succeed.
		bool is_full();

		/// Updates the currently-bound texture with all new data provided since the last @c submit.
		void submit();

	private:
		// where pixel data will be put to the next time a write is requested
		uint16_t next_write_x_position_, next_write_y_position_;

		// the most recent position returned for pixel data writing
		uint16_t write_x_position_, write_y_position_;

		// details of the most recent allocation
		size_t write_target_pointer_;
		size_t last_allocation_amount_;

		// the buffer size
		size_t bytes_per_pixel_;

		// the buffer
		std::vector<uint8_t> image_;
		GLuint texture_name_;
};

}
}

#endif /* CRTInputBufferBuilder_hpp */
