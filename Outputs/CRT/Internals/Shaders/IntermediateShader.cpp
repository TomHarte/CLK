//
//  IntermediateShader.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 28/04/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "IntermediateShader.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "../../../../SignalProcessing/FIRFilter.hpp"

using namespace OpenGL;

namespace {
	const OpenGL::Shader::AttributeBinding bindings[] = {
		{"inputPosition", 0},
		{"outputPosition", 1},
		{"phaseAndAmplitude", 2},
		{"phaseTime", 3},
		{nullptr}
	};
}

std::unique_ptr<IntermediateShader> IntermediateShader::make_shader(const std::string &fragment_shader, bool use_usampler, bool input_is_inputPosition) {
	const char *sampler_type = use_usampler ? "usampler2D" : "sampler2D";
	const char *input_variable = input_is_inputPosition ? "inputPosition" : "outputPosition";

	char *vertex_shader;
	asprintf(&vertex_shader,
		"#version 150\n"

		"in vec2 inputStart;"
		"in vec2 outputStart;"
		"in vec2 ends;"
		"in vec3 phaseTimeAndAmplitude;"

		"uniform ivec2 outputTextureSize;"
		"uniform float extension;"
		"uniform %s texID;"
		"uniform float offsets[5];"
		"uniform vec2 widthScalers;"
		"uniform float inputVerticalOffset;"
		"uniform float outputVerticalOffset;"
		"uniform float textureHeightDivisor;"

		"out vec3 phaseAndAmplitudeVarying;"
		"out vec2 inputPositionsVarying[11];"
		"out vec2 iInputPositionVarying;"
		"out vec2 delayLinePositionVarying;"

		"void main(void)"
		"{"
			// odd vertices are on the left, even on the right
			"float extent = float(gl_VertexID & 1);"
			"float longitudinal = float((gl_VertexID & 2) >> 1);"

			// inputPosition.x is either inputStart.x or ends.x, depending on whether it is on the left or the right;
			// outputPosition.x is either outputStart.x or ends.y;
			// .ys are inputStart.y and outputStart.y respectively
			"vec2 inputPosition = vec2(mix(inputStart.x, ends.x, extent)*widthScalers[0], inputStart.y + inputVerticalOffset);"
			"vec2 outputPosition = vec2(mix(outputStart.x, ends.y, extent)*widthScalers[1], outputStart.y + outputVerticalOffset);"

			"inputPosition.y += longitudinal;"
			"outputPosition.y += longitudinal;"

			// extension is the amount to extend both the input and output by to add a full colour cycle at each end
			"vec2 extensionVector = vec2(extension, 0.0) * 2.0 * (extent - 0.5);"

			// extended[Input/Output]Position are [input/output]Position with the necessary applied extension
			"vec2 extendedInputPosition = %s + extensionVector;"
			"vec2 extendedOutputPosition = outputPosition + extensionVector;"

			// keep iInputPositionVarying in whole source pixels, scale mappedInputPosition to the ordinary normalised range
			"vec2 textureSize = vec2(textureSize(texID, 0));"
			"iInputPositionVarying = extendedInputPosition;"
			"vec2 mappedInputPosition = extendedInputPosition / textureSize;"	//  + vec2(0.0, 0.5)

			// setup input positions spaced as per the supplied offsets; these are for filtering where required
			"inputPositionsVarying[0] = mappedInputPosition - (vec2(5.0, 0.0) / textureSize);"
			"inputPositionsVarying[1] = mappedInputPosition - (vec2(4.0, 0.0) / textureSize);"
			"inputPositionsVarying[2] = mappedInputPosition - (vec2(3.0, 0.0) / textureSize);"
			"inputPositionsVarying[3] = mappedInputPosition - (vec2(2.0, 0.0) / textureSize);"
			"inputPositionsVarying[4] = mappedInputPosition - (vec2(1.0, 0.0) / textureSize);"
			"inputPositionsVarying[5] = mappedInputPosition;"
			"inputPositionsVarying[6] = mappedInputPosition + (vec2(1.0, 0.0) / textureSize);"
			"inputPositionsVarying[7] = mappedInputPosition + (vec2(2.0, 0.0) / textureSize);"
			"inputPositionsVarying[8] = mappedInputPosition + (vec2(3.0, 0.0) / textureSize);"
			"inputPositionsVarying[9] = mappedInputPosition + (vec2(4.0, 0.0) / textureSize);"
			"inputPositionsVarying[10] = mappedInputPosition + (vec2(5.0, 0.0) / textureSize);"
			"delayLinePositionVarying = mappedInputPosition - vec2(0.0, 1.0);"

			// setup phaseAndAmplitudeVarying.x as colour burst subcarrier phase, in radians;
			// setup phaseAndAmplitudeVarying.y as colour burst amplitude;
			// setup phaseAndAmplitudeVarying.z as 1 / (colour burst amplitude), or 0.0 if amplitude is 0.0;
			"phaseAndAmplitudeVarying.x = (extendedOutputPosition.x + (phaseTimeAndAmplitude.x / 64.0)) * 0.5 * 3.141592654;"
			"phaseAndAmplitudeVarying.y = phaseTimeAndAmplitude.y / 255.0;"
			"phaseAndAmplitudeVarying.z = (phaseAndAmplitudeVarying.y > 0.0) ? 1.0 / phaseAndAmplitudeVarying.y : 0.0;"

			// determine output position by scaling the output position according to the texture size
			"vec2 eyePosition = 2.0*(extendedOutputPosition / outputTextureSize) - vec2(1.0);"
			"gl_Position = vec4(eyePosition, 0.0, 1.0);"
		"}", sampler_type, input_variable);

	std::unique_ptr<IntermediateShader> shader(new IntermediateShader(vertex_shader, fragment_shader, bindings));
	free(vertex_shader);

	return shader;
}

std::unique_ptr<IntermediateShader> IntermediateShader::make_source_conversion_shader(const std::string &composite_shader, const std::string &rgb_shader) {
	char *derived_composite_sample = nullptr;
	const char *composite_sample = composite_shader.c_str();
	if(!composite_shader.size()) {
		asprintf(&derived_composite_sample,
			"%s\n"
			"uniform mat3 rgbToLumaChroma;"
			"float composite_sample(usampler2D texID, vec2 coordinate, vec2 iCoordinate, float phase, float amplitude)"
			"{"
				"vec3 rgbColour = clamp(rgb_sample(texID, coordinate, iCoordinate), vec3(0.0), vec3(1.0));"
				"vec3 lumaChromaColour = rgbToLumaChroma * rgbColour;"
				"vec2 quadrature = vec2(cos(phase), -sin(phase)) * amplitude;"
				"return dot(lumaChromaColour, vec3(1.0 - amplitude, quadrature));"
			"}",
			rgb_shader.c_str());
		composite_sample = derived_composite_sample;
	}

	char *fragment_shader;
	asprintf(&fragment_shader,
		"#version 150\n"

		"in vec2 inputPositionsVarying[11];"
		"in vec2 iInputPositionVarying;"
		"in vec3 phaseAndAmplitudeVarying;"

		"out vec4 fragColour;"

		"uniform usampler2D texID;"

		"\n%s\n"

		"void main(void)"
		"{"
			"fragColour = vec4(composite_sample(texID, inputPositionsVarying[5], iInputPositionVarying, phaseAndAmplitudeVarying.x, phaseAndAmplitudeVarying.y));"
		"}"
	, composite_sample);
	free(derived_composite_sample);

	std::unique_ptr<IntermediateShader> shader = make_shader(fragment_shader, true, true);
	free(fragment_shader);

	return shader;
}

std::unique_ptr<IntermediateShader> IntermediateShader::make_rgb_source_shader(const std::string &rgb_shader) {
	char *fragment_shader;
	asprintf(&fragment_shader,
		"#version 150\n"

		"in vec2 inputPositionsVarying[11];"
		"in vec2 iInputPositionVarying;"
		"in vec3 phaseAndAmplitudeVarying;"

		"out vec3 fragColour;"

		"uniform usampler2D texID;"

		"\n%s\n"

		"void main(void)"
		"{"
			"fragColour = rgb_sample(texID, inputPositionsVarying[5], iInputPositionVarying);"
		"}"
	, rgb_shader.c_str());

	std::unique_ptr<IntermediateShader> shader = make_shader(fragment_shader, true, true);
	free(fragment_shader);

	return shader;
}

std::unique_ptr<IntermediateShader> IntermediateShader::make_chroma_luma_separation_shader() {
	return make_shader(
		"#version 150\n"

		"in vec3 phaseAndAmplitudeVarying;"
		"in vec2 inputPositionsVarying[11];"

		"out vec3 fragColour;"

		"uniform sampler2D texID;"

		"void main(void)"
		"{"
			"vec4 samples = vec4("
				"texture(texID, inputPositionsVarying[3]).r,"
				"texture(texID, inputPositionsVarying[4]).r,"
				"texture(texID, inputPositionsVarying[5]).r,"
				"texture(texID, inputPositionsVarying[6]).r"
			");"
			"float luminance = dot(samples, vec4(0.25));"

			// define chroma to be whatever was here, minus luma
			"float chrominance = 0.5 * (samples.z - luminance) * phaseAndAmplitudeVarying.z;"
			"luminance /= (1.0 - phaseAndAmplitudeVarying.y);"

			// split choma colours here, as the most direct place, writing out
			// RGB = (luma, chroma.x, chroma.y)
			"vec2 quadrature = vec2(cos(phaseAndAmplitudeVarying.x), -sin(phaseAndAmplitudeVarying.x));"
			"fragColour = vec3(luminance, vec2(0.5) + (chrominance * quadrature));"
		"}",false, false);
}

std::unique_ptr<IntermediateShader> IntermediateShader::make_chroma_filter_shader() {
	return make_shader(
		"#version 150\n"

		"in vec2 inputPositionsVarying[11];"
		"uniform vec4 weights[3];"

		"out vec3 fragColour;"

		"uniform sampler2D texID;"
		"uniform mat3 lumaChromaToRGB;"

		"void main(void)"
		"{"
			"vec3 samples[] = vec3[]("
				"texture(texID, inputPositionsVarying[3]).rgb,"
				"texture(texID, inputPositionsVarying[4]).rgb,"
				"texture(texID, inputPositionsVarying[5]).rgb,"
				"texture(texID, inputPositionsVarying[6]).rgb"
			");"

			"vec4 chromaChannel1 = vec4(samples[0].g, samples[1].g, samples[2].g, samples[3].g);"
			"vec4 chromaChannel2 = vec4(samples[0].b, samples[1].b, samples[2].b, samples[3].b);"

			"vec3 lumaChromaColour = vec3(samples[2].r,"
				"dot(chromaChannel1, vec4(0.25)),"
				"dot(chromaChannel2, vec4(0.25))"
			");"

			"vec3 lumaChromaColourInRange = (lumaChromaColour - vec3(0.0, 0.5, 0.5)) * vec3(1.0, 2.0, 2.0);"
			"fragColour = lumaChromaToRGB * lumaChromaColourInRange;"
		"}", false, false);
}

std::unique_ptr<IntermediateShader> IntermediateShader::make_rgb_filter_shader() {
	return make_shader(
		"#version 150\n"

		"in vec2 inputPositionsVarying[11];"
		"uniform vec4 weights[3];"

		"out vec3 fragColour;"

		"uniform sampler2D texID;"

		"void main(void)"
		"{"
			"vec3 samples[] = vec3[]("
				"texture(texID, inputPositionsVarying[0]).rgb,"
				"texture(texID, inputPositionsVarying[1]).rgb,"
				"texture(texID, inputPositionsVarying[2]).rgb,"
				"texture(texID, inputPositionsVarying[3]).rgb,"
				"texture(texID, inputPositionsVarying[4]).rgb,"
				"texture(texID, inputPositionsVarying[5]).rgb,"
				"texture(texID, inputPositionsVarying[6]).rgb,"
				"texture(texID, inputPositionsVarying[7]).rgb,"
				"texture(texID, inputPositionsVarying[8]).rgb,"
				"texture(texID, inputPositionsVarying[9]).rgb,"
				"texture(texID, inputPositionsVarying[10]).rgb"
			");"

			"vec4 channel1[] = vec4[]("
				"vec4(samples[0].r, samples[1].r, samples[2].r, samples[3].r),"
				"vec4(samples[4].r, samples[5].r, samples[6].r, samples[7].r),"
				"vec4(samples[8].r, samples[9].r, samples[10].r, 0.0)"
			");"
			"vec4 channel2[] = vec4[]("
				"vec4(samples[0].g, samples[1].g, samples[2].g, samples[3].g),"
				"vec4(samples[4].g, samples[5].g, samples[6].g, samples[7].g),"
				"vec4(samples[8].g, samples[9].g, samples[10].g, 0.0)"
			");"
			"vec4 channel3[] = vec4[]("
				"vec4(samples[0].b, samples[1].b, samples[2].b, samples[3].b),"
				"vec4(samples[4].b, samples[5].b, samples[6].b, samples[7].b),"
				"vec4(samples[8].b, samples[9].b, samples[10].b, 0.0)"
			");"

			"fragColour = vec3("
				"dot(vec3("
					"dot(channel1[0], weights[0]),"
					"dot(channel1[1], weights[1]),"
					"dot(channel1[2], weights[2])"
				"), vec3(1.0)),"
				"dot(vec3("
					"dot(channel2[0], weights[0]),"
					"dot(channel2[1], weights[1]),"
					"dot(channel2[2], weights[2])"
				"), vec3(1.0)),"
				"dot(vec3("
					"dot(channel3[0], weights[0]),"
					"dot(channel3[1], weights[1]),"
					"dot(channel3[2], weights[2])"
				"), vec3(1.0))"
			");"
		"}", false, false);
}

void IntermediateShader::set_output_size(unsigned int output_width, unsigned int output_height) {
	set_uniform("outputTextureSize", (GLint)output_width, (GLint)output_height);
}

void IntermediateShader::set_source_texture_unit(GLenum unit) {
	set_uniform("texID", (GLint)(unit - GL_TEXTURE0));
}

void IntermediateShader::set_filter_coefficients(float sampling_rate, float cutoff_frequency) {
	// The process below: the source texture will have bilinear filtering enabled; so by
	// sampling at non-integral offsets from the centre the shader can get a weighted sum
	// of two source pixels, then scale that once, to do two taps per sample. However
	// that works only if the two coefficients being joined have the same sign. So the
	// number of usable taps is between 11 and 21 depending on the values that come out.
	// Perform a linear search for the highest number of taps we can use with 11 samples.
	GLfloat weights[12];
	GLfloat offsets[5];
	unsigned int taps = 11;
//	unsigned int taps = 21;
	while(1) {
		float coefficients[21];
		SignalProcessing::FIRFilter luminance_filter(taps, sampling_rate, 0.0f, cutoff_frequency, SignalProcessing::FIRFilter::DefaultAttenuation);
		luminance_filter.get_coefficients(coefficients);

//		int sample = 0;
//		int c = 0;
		memset(weights, 0, sizeof(float)*12);
		memset(offsets, 0, sizeof(float)*5);

		int halfSize = (taps >> 1);
		for(int c = 0; c < taps; c++) {
			if(c < 5) offsets[c] = (halfSize - c);
			weights[c] = coefficients[c];
		}
		break;

//		int halfSize = (taps >> 1);
//		while(c < halfSize && sample < 5) {
//			offsets[sample] = (float)(halfSize - c);
//			if((coefficients[c] < 0.0f) == (coefficients[c+1] < 0.0f) && c+1 < (taps >> 1)) {
//				weights[sample] = coefficients[c] + coefficients[c+1];
//				offsets[sample] -= (coefficients[c+1] / weights[sample]);
//				c += 2;
//			} else {
//				weights[sample] = coefficients[c];
//				c++;
//			}
//			sample ++;
//		}
//		if(c == halfSize) {	// i.e. we finished combining inputs before we ran out of space
//			weights[sample] = coefficients[c];
//			for(int c = 0; c < sample; c++) {
//				weights[sample+c+1] = weights[sample-c-1];
//			}
//			break;
//		}
		taps -= 2;
	}

	set_uniform("weights", 4, 3, weights);
	set_uniform("offsets", 1, 5, offsets);
}

void IntermediateShader::set_separation_frequency(float sampling_rate, float colour_burst_frequency) {
	set_filter_coefficients(sampling_rate, colour_burst_frequency);
}

void IntermediateShader::set_extension(float extension) {
	set_uniform("extension", extension);
}

void IntermediateShader::set_colour_conversion_matrices(float *fromRGB, float *toRGB) {
	set_uniform_matrix("lumaChromaToRGB", 3, false, toRGB);
	set_uniform_matrix("rgbToLumaChroma", 3, false, fromRGB);
}

void IntermediateShader::set_width_scalers(float input_scaler, float output_scaler) {
	set_uniform("widthScalers", input_scaler, output_scaler);
}

void IntermediateShader::set_is_double_height(bool is_double_height, float input_offset, float output_offset) {
	set_uniform("textureHeightDivisor", is_double_height ? 2.0f : 1.0f);
	set_uniform("inputVerticalOffset", input_offset);
	set_uniform("outputVerticalOffset", output_offset);
}
