//
//  OutputShader.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 27/04/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef OutputShader_hpp
#define OutputShader_hpp

#include "Shader.hpp"
#include "../../CRTTypes.hpp"
#include <memory>

namespace OpenGL {

class OutputShader: public Shader {
public:
	/*!
		Constructs and returns an instance of OutputShader. OutputShaders are intended to read source data
		from a texture and draw a single raster scan containing that data as output.

		Does not catch any of the exceptions potentially thrown by `Shader::Shader`.

		All instances of OutputShader are guaranteed to use the same attribute locations for their inputs.

		@param fragment_methods A block of code that will appear within the global area of the fragment shader.

		@param colour_expression An expression that should evaluate to a `vec3` indicating the colour at the current location. The
		decision should be a function of the uniform `texID`, which will be either a `usampler2D` or a `sampler2D` as per the
		`use_usampler` parameter, and the inputs `srcCoordinatesVarying` which is a location within the texture from which to
		take the source value, and `iSrcCoordinatesVarying` which is a value proportional to `srcCoordinatesVarying` but scaled
		so that one unit equals one source sample.

		@param use_usampler Dictates the type of the `texID` uniform; will be a `usampler2D` if this parameter is `true`, a
		`sampler2D` otherwise.

		@returns an instance of OutputShader.
	*/
	static std::unique_ptr<OutputShader> make_shader(const char *fragment_methods, const char *colour_expression, bool use_usampler);
	using Shader::Shader;

	/*!
		Queues configuration for output to an area of `output_width` and `output_height` pixels, ensuring
		the largest possible drawing size that allows everything within `visible_area` to be visible, to
		occur upon the next `bind`.
	*/
	void set_output_size(unsigned int output_width, unsigned int output_height, Outputs::CRT::Rect visible_area);

	/*!
		Queues setting of the texture unit (as an enum, e.g. `GL_TEXTURE0`) for source data upon the next `bind`.
	*/
	void set_source_texture_unit(GLenum unit);

	/*!
		Queues configuring this shader's understanding of how to map from the source vertex stream to screen coordinates,
		to occur upon the next `bind`.
	*/
	void set_timing(unsigned int height_of_display, unsigned int cycles_per_line, unsigned int horizontal_scan_period, unsigned int vertical_scan_period, unsigned int vertical_period_divider);

	/*!
	*/
	void set_origin_is_double_height(bool is_double_height);

	/*!
		Sets the proportion of the input area that should be considered the whole width — 1.0 means use all available
		space, 0.5 means use half, etc.
	*/
	void set_input_width_scaler(float input_scaler);
};

}

#endif /* OutputShader_hpp */
