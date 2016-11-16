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
	Owns an OpenGL texture resource and provides mechanisms to fill it from top left to bottom right
	with runs of data, ensuring each run is neighboured immediately to the left and right by copies of its
	first and last pixels.
*/
class TextureBuilder {
	public:
		/// Constructs an instance of InputTextureBuilder that contains a texture of colour depth @c bytes_per_pixel.
		TextureBuilder(size_t bytes_per_pixel);

		/// Finds the first available space of at least @c required_length pixels in size. Calls must be paired off
		/// with calls to @c reduce_previous_allocation_to.
		/// @returns a pointer to the allocated space if any was available; @c nullptr otherwise.
		uint8_t *allocate_write_area(size_t required_length);

		/// Announces that the owner is finished with the region created by the most recent @c allocate_write_area
		/// and indicates that its actual final size was @c actual_length.
		void reduce_previous_allocation_to(size_t actual_length);

		/// @returns the row that was the final one to receive data; also resets the builder to restart filling of
		/// the texture from row 0.
		uint16_t get_and_finalise_current_line();

		/// @returns a pointer to the image data for this texture.
		uint8_t *get_image_pointer();

		/// @returns the start column for the most recent allocated write area.
		uint16_t get_last_write_x_position();

		/// @returns the row of the most recent allocated write area.
		uint16_t get_last_write_y_position();

		/// @returns the number of bytes per pixel as supplied to the constructor.
		size_t get_bytes_per_pixel();

		/// @returns @c true if all future calls to @c allocate_write_area will fail on account of the input texture
		/// being full; @c false if calls may succeed.
		bool is_full();

	private:
		// where pixel data will be put to the next time a write is requested
		uint16_t _next_write_x_position, _next_write_y_position;

		// the most recent position returned for pixel data writing
		uint16_t _write_x_position, _write_y_position;

		// details of the most recent allocation
		size_t _write_target_pointer;
		size_t _last_allocation_amount;

		// the buffer size
		size_t _bytes_per_pixel;

		// the buffer
		std::vector<uint8_t> _image;
};

}
}

#endif /* CRTInputBufferBuilder_hpp */
