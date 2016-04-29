//
//  IntermediateShader.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 28/04/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef IntermediateShader_hpp
#define IntermediateShader_hpp

#include <stdio.h>

#include "Shader.hpp"
#include <memory>

namespace OpenGL {

class IntermediateShader: public Shader {
public:
	using Shader::Shader;

	static std::unique_ptr<IntermediateShader> make_source_conversion_shader(const char *composite_shader, const char *rgb_shader);
	static std::unique_ptr<IntermediateShader> make_chroma_luma_separation_shader();
	static std::unique_ptr<IntermediateShader> make_chroma_filter_shader();

	/*!
		Binds this shader and configures it for output to an area of `output_width` and `output_height` pixels, ensuring
		the largest possible drawing size that allows everything within `visible_area` to be visible.
	*/
	void set_output_size(unsigned int output_width, unsigned int output_height);

	/*!
		Binds this shader and sets the texture unit (as an enum, e.g. `GL_TEXTURE0`) to sample as source data.
	*/
	void set_source_texture_unit(GLenum unit);

	void set_filter_coefficients(float sampling_rate, float cutoff_frequency);

	void set_phase_cycles_per_sample(float phase_cycles_per_sample, bool extend_runs_to_full_cycle);

	void set_colour_conversion_matrices(float *fromRGB, float *toRGB);

private:
	static std::unique_ptr<IntermediateShader> make_shader(const char *fragment_shader, bool use_usampler, bool input_is_inputPosition);

	GLint texIDUniform;
	GLint outputTextureSizeUniform;
	GLint weightsUniform;
	GLint phaseCyclesPerTickUniform;
	GLint extensionUniform;
	GLint rgbToLumaChromaUniform;
	GLint lumaChromaToRGBUniform;
};

}

#endif /* IntermediateShader_hpp */
