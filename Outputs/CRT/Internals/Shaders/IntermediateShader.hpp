//
//  IntermediateShader.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 28/04/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#ifndef IntermediateShader_hpp
#define IntermediateShader_hpp

#include "Shader.hpp"

#include <cstdio>
#include <memory>

namespace OpenGL {

class IntermediateShader: public Shader {
public:
	using Shader::Shader;

	enum class Input {
		/// Contains the 2d start position of this run's input data.
		InputStart,
		/// Contains the 2d start position of this run's output position.
		OutputStart,
		/// A 2d vector comprised of (the final x position for input, the final x position for output).
		Ends,
		/// A 3d vector recording the colour subcarrier's (phase, time, amplitude) at the start of this span of data.
		PhaseTimeAndAmplitude
	};

	/*!
		Obtains the name of a designated input. Designated inputs are guaranteed to have the same attribute location
		across multiple instances of IntermediateShader. So binding a vertex array to these inputs for any instance of
		IntermediateShader allows that array to work with all instances of IntermediateShader.

		@param input The input to query.
		@returns The name used in this shader's source for the nominated input.
	*/
	static std::string get_input_name(Input input);

	/*!
		Constructs and returns an intermediate shader that will take runs from the inputPositions,
		converting them to single-channel composite values using @c composite_shader if non-empty
		or a reference composite conversion of @c svideo_shader (first preference) or
		@c rgb_shader (second preference) otherwise.

		[input format] => one-channel composite.
	*/
	static std::unique_ptr<IntermediateShader> make_composite_source_shader(const std::string &composite_shader, const std::string &svideo_shader, const std::string &rgb_shader);

	/*!
		Constructs and returns an intermediate shader that will take runs from the inputPositions,
		converting them to two-channel svideo values using @c svideo_shader if non-empty
		or a reference svideo conversion of @c rgb_shader otherwise.

		[input format] => three-channel Y, noisy (m, n).
	*/
	static std::unique_ptr<IntermediateShader> make_svideo_source_shader(const std::string &svideo_shader, const std::string &rgb_shader);

	/*!
		Constructs and returns an intermediate shader that will take runs from the inputPositions,
		converting them to RGB values using @c rgb_shader.

		[input format] => three-channel RGB.
	*/
	static std::unique_ptr<IntermediateShader> make_rgb_source_shader(const std::string &rgb_shader);

	/*!
		Constructs and returns an intermediate shader that will read composite samples from the R channel,
		filter then to obtain luminance, stored to R, and to separate out unfiltered chrominance, store to G and B.

		one-channel composite => three-channel Y, noisy (m, n).
	*/
	static std::unique_ptr<IntermediateShader> make_chroma_luma_separation_shader();

	/*!
		Constructs and returns an intermediate shader that will pass R through unchanged while filtering G and B.

		three-channel Y, noisy (m, n) => three-channel RGB.
	*/
	static std::unique_ptr<IntermediateShader> make_chroma_filter_shader();

	/*!
		Constructs and returns an intermediate shader that will filter R, G and B.

		three-channel RGB => frequency-limited three-channel RGB.
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
	void set_extension(float extension);

	/*!
		Queues setting the matrices that convert between RGB and chrominance/luminance to occur on the next `bind`.
	*/
	void set_colour_conversion_matrices(float *fromRGB, float *toRGB);

	/*!
		Sets the proportions of the input and output areas that should be considered the whole width: 1.0 means use all available
		space, 0.5 means use half, etc.
	*/
	void set_width_scalers(float input_scaler, float output_scaler);

	/*!
		Sets source and target vertical offsets.
	*/
	void set_is_double_height(bool is_double_height, float input_offset = 0.0f, float output_offset = 0.0f);

	/*!
		Sets the multiplier applied in the vertex shader to iCoordinates.
	*/
	void set_integer_coordinate_multiplier(float);

private:
	static std::unique_ptr<IntermediateShader> make_shader(const std::string &fragment_shader, bool use_usampler, bool input_is_inputPosition);
};

}

#endif /* IntermediateShader_hpp */
