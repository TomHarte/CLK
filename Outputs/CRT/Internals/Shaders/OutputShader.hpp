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

		The fragment shader should expect to receive the inputs:

			in float lateralVarying;
			in vec2 srcCoordinatesVarying;
			in vec2 iSrcCoordinatesVarying;

		If `use_usampler` is `true` then a `uniform usampler2D texID` will be used as the source texture.
		Otherwise it'll be a `sampler2D`.

		`lateralVarying` is a value in radians that is equal to 0 at the centre of the scan and ± a suitable
		angle at the top and bottom extremes.

		`srcCoordinatesVarying` is a value representing the coordinates for source data in the texture
		attached to the sampler `texID`.

		`iSrcCoordinatesVarying` is a value corresponding to `srcCoordinatesVarying` but scaled up so that
		one unit is the width of one source sample.

		Does not catch any exceptions raised by `Shader::Shader`.

		@returns an instance of OutputShader.
	*/
	static std::unique_ptr<OutputShader> make_shader(const char *fragment_methods, const char *output_colour, bool use_usampler);
	using Shader::Shader;

	void set_output_size(unsigned int output_width, unsigned int output_height, Outputs::CRT::Rect visible_area);
	void set_source_texture_unit(GLenum unit);
	void set_timing(unsigned int height_of_display, unsigned int cycles_per_line, unsigned int horizontal_scan_period, unsigned int vertical_scan_period, unsigned int vertical_period_divider);

	private:
		GLint boundsOriginUniform, boundsSizeUniform, texIDUniform, scanNormalUniform, positionConversionUniform;
};

}

#endif /* OutputShader_hpp */
