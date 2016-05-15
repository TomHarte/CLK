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

	/*!
		Constructs and returns an intermediate shader that will take runs from the inputPositions,
		converting them to single-channel composite values using @c composite_shader if supplied
		or @c rgb_shader and a reference composite conversion if @c composite_shader is @c nullptr.
	*/
	static std::unique_ptr<IntermediateShader> make_source_conversion_shader(const char *composite_shader, const char *rgb_shader);

	/*!
		Constructs and returns an intermediate shader that will take runs from the inputPositions,
		converting them to RGB values using @c rgb_shader.
	*/
	static std::unique_ptr<IntermediateShader> make_rgb_source_shader(const char *rgb_shader);

	/*!
		Constructs and returns an intermediate shader that will read composite samples from the R channel,
		filter then to obtain luminance, stored to R, and to separate out unfiltered chrominance, store to G and B.
	*/
	static std::unique_ptr<IntermediateShader> make_chroma_luma_separation_shader();

	/*!
		Constructs and returns an intermediate shader that will pass R through unchanged while filtering G and B.
	*/
	static std::unique_ptr<IntermediateShader> make_chroma_filter_shader();

	/*!
		Constructs and returns an intermediate shader that will filter R while passing through G and B unchanged.
	*/
	static std::unique_ptr<IntermediateShader> make_luma_filter_shader();

	/*!
		Constructs and returns an intermediate shader that will filter R, G and B.
	*/
	static std::unique_ptr<IntermediateShader> make_rgb_filter_shader();

	/*!
		Queues the configuration of this shader for output to an area of `output_width` and `output_height` pixels
		to occur upon the next `bind`.
	*/
	void set_output_size(unsigned int output_width, unsigned int output_height);

	/*!
		Queues setting the texture unit (as an enum, e.g. `GL_TEXTURE0`) for source data to occur upon the next `bind`.
	*/
	void set_source_texture_unit(GLenum unit);

	/*!
		Queues setting filtering coefficients for a lowpass filter based on the cutoff frequency to occur upon the next `bind`.
	*/
	void set_filter_coefficients(float sampling_rate, float cutoff_frequency);

	/*!
		Queues configuration of filtering to separate luminance and chrominance based on a colour
		subcarrier of the given frequency to occur upon the next `bind`.
	*/
	void set_separation_frequency(float sampling_rate, float colour_burst_frequency);

	/*!
		Queues setting of the number of colour phase cycles per sample, indicating whether output
		geometry should be extended so that a complete colour cycle is included at both the beginning and end,
		to occur upon the next `bind`.
	*/
	void set_phase_cycles_per_sample(float phase_cycles_per_sample, bool extend_runs_to_full_cycle);

	/*!
		Queues setting the matrices that convert between RGB and chrominance/luminance to occur on the next `bind`.
	*/
	void set_colour_conversion_matrices(float *fromRGB, float *toRGB);

private:
	static std::unique_ptr<IntermediateShader> make_shader(const char *fragment_shader, bool use_usampler, bool input_is_inputPosition);
};

}

#endif /* IntermediateShader_hpp */
