//
//  Screenshot.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 27/02/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#pragma once

#include "OpenGL.hpp"

#include <algorithm>
#include <vector>

namespace Outputs::Display::OpenGL {

/*!
	Upon construction, Screenshot will capture the centre portion of the currently-bound framebuffer,
	cropping to an image that matches the requested aspect ratio.

	The image will then be available as RGBA data, in raster order via the struct members.
*/
struct Screenshot {
	Screenshot(const int aspect_width, const int aspect_height) {
		// Get the current viewport to establish framebuffer size. Then determine how wide the
		// centre portion of that would be, allowing for the requested aspect ratio.
		GLint dimensions[4];
		glGetIntegerv(GL_VIEWPORT, dimensions);

		height = int(dimensions[3]);
		width = (height * aspect_width) / aspect_height;
		pixel_data.resize(size_t(width * height * 4));

		// Grab the framebuffer contents, temporarily setting single-byte alignment.
		int prior_alignment;
		glGetIntegerv(GL_PACK_ALIGNMENT, &prior_alignment);
		glReadPixels(
			(dimensions[2] - GLint(width)) >> 1,
			0,
			GLint(width),
			GLint(height),
			GL_RGBA,
			GL_UNSIGNED_BYTE,
			pixel_data.data()
		);
		glPixelStorei(GL_PACK_ALIGNMENT, prior_alignment);

		// Flip the contents into raster order.
		const size_t line_size = size_t(width * 4);
		for(size_t y = 0; y < size_t(height) / 2; ++y) {
			const size_t flipped_y = size_t(height - 1) - y;
			std::swap_ranges(
				&pixel_data[flipped_y * line_size],
				&pixel_data[flipped_y * line_size + line_size],
				&pixel_data[y * line_size]
			);
		}
	}

	std::vector<uint8_t> pixel_data;
	int width, height;
};

}
