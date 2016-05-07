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
	const OpenGL::Shader::AttributeBinding bindings[] =
	{
		{"inputPosition", 0},
		{"outputPosition", 1},
		{"phaseAndAmplitude", 2},
		{"phaseTime", 3},
		{nullptr}
	};
}

std::unique_ptr<IntermediateShader> IntermediateShader::make_shader(const char *fragment_shader, bool use_usampler, bool input_is_inputPosition)
{
	const char *sampler_type = use_usampler ? "usampler2D" : "sampler2D";
	const char *input_variable = input_is_inputPosition ? "inputPosition" : "outputPosition";

	char *vertex_shader;
	asprintf(&vertex_shader,
		"#version 150\n"

		"in vec2 inputPosition;"
		"in vec2 outputPosition;"
		"in vec2 phaseAndAmplitude;"
		"in float phaseTime;"

		"uniform float phaseCyclesPerTick;"
		"uniform ivec2 outputTextureSize;"
		"uniform float extension;"
		"uniform %s texID;"
		"uniform float offsets[5];"

		"out vec2 phaseAndAmplitudeVarying;"
		"out vec2 inputPositionsVarying[11];"
		"out vec2 iInputPositionVarying;"
		"out vec2 delayLinePositionVarying;"

		"void main(void)"
		"{"
			"float direction = float(gl_VertexID & 1);"
			"vec2 extensionVector = vec2(extension, 0.0) * 2.0 * (direction - 0.5);"
			"vec2 extendedInputPosition = %s + extensionVector;"
			"vec2 extendedOutputPosition = outputPosition + extensionVector;"

			"vec2 textureSize = vec2(textureSize(texID, 0));"
			"iInputPositionVarying = extendedInputPosition;"
			"vec2 mappedInputPosition = (extendedInputPosition + vec2(0.0, 0.5)) / textureSize;"

			"inputPositionsVarying[0] = mappedInputPosition - (vec2(offsets[0], 0.0) / textureSize);"
			"inputPositionsVarying[1] = mappedInputPosition - (vec2(offsets[1], 0.0) / textureSize);"
			"inputPositionsVarying[2] = mappedInputPosition - (vec2(offsets[2], 0.0) / textureSize);"
			"inputPositionsVarying[3] = mappedInputPosition - (vec2(offsets[3], 0.0) / textureSize);"
			"inputPositionsVarying[4] = mappedInputPosition - (vec2(offsets[4], 0.0) / textureSize);"
			"inputPositionsVarying[5] = mappedInputPosition;"
			"inputPositionsVarying[6] = mappedInputPosition + (vec2(offsets[4], 0.0) / textureSize);"
			"inputPositionsVarying[7] = mappedInputPosition + (vec2(offsets[3], 0.0) / textureSize);"
			"inputPositionsVarying[8] = mappedInputPosition + (vec2(offsets[2], 0.0) / textureSize);"
			"inputPositionsVarying[9] = mappedInputPosition + (vec2(offsets[1], 0.0) / textureSize);"
			"inputPositionsVarying[10] = mappedInputPosition + (vec2(offsets[0], 0.0) / textureSize);"
			"delayLinePositionVarying = mappedInputPosition - vec2(0.0, 1.0);"

			"phaseAndAmplitudeVarying.x = (phaseCyclesPerTick * (extendedOutputPosition.x - phaseTime) + phaseAndAmplitude.x) * 2.0 * 3.141592654;"
			"phaseAndAmplitudeVarying.y = 0.33;" // TODO: reinstate connection with phaseAndAmplitude

			"vec2 eyePosition = 2.0*(extendedOutputPosition / outputTextureSize) - vec2(1.0) + vec2(0.5)/textureSize;"
			"gl_Position = vec4(eyePosition, 0.0, 1.0);"
		"}", sampler_type, input_variable);

	std::unique_ptr<IntermediateShader> shader = std::unique_ptr<IntermediateShader>(new IntermediateShader(vertex_shader, fragment_shader, bindings));
	free(vertex_shader);

	shader->texIDUniform				= shader->get_uniform_location("texID");
	shader->outputTextureSizeUniform	= shader->get_uniform_location("outputTextureSize");
	shader->phaseCyclesPerTickUniform	= shader->get_uniform_location("phaseCyclesPerTick");
	shader->extensionUniform			= shader->get_uniform_location("extension");
	shader->weightsUniform				= shader->get_uniform_location("weights");
	shader->rgbToLumaChromaUniform		= shader->get_uniform_location("rgbToLumaChroma");
	shader->lumaChromaToRGBUniform		= shader->get_uniform_location("lumaChromaToRGB");
	shader->offsetsUniform				= shader->get_uniform_location("offsets");

	return shader;
}

std::unique_ptr<IntermediateShader> IntermediateShader::make_source_conversion_shader(const char *composite_shader, const char *rgb_shader)
{
	char *composite_sample = (char *)composite_shader;
	if(!composite_sample)
	{
		asprintf(&composite_sample,
			"%s\n"
			"uniform mat3 rgbToLumaChroma;"
			"float composite_sample(usampler2D texID, vec2 coordinate, vec2 iCoordinate, float phase, float amplitude)"
			"{"
				"vec3 rgbColour = clamp(rgb_sample(texID, coordinate, iCoordinate), vec3(0.0), vec3(1.0));"
				"vec3 lumaChromaColour = rgbToLumaChroma * rgbColour;"
				"vec2 quadrature = vec2(cos(phase), -sin(phase)) * amplitude;"
				"return dot(lumaChromaColour, vec3(1.0 - amplitude, quadrature));"
			"}",
			rgb_shader);
	}

	char *fragment_shader;
	asprintf(&fragment_shader,
		"#version 150\n"

		"in vec2 inputPositionsVarying[11];"
		"in vec2 iInputPositionVarying;"
		"in vec2 phaseAndAmplitudeVarying;"

		"out vec4 fragColour;"

		"uniform usampler2D texID;"

		"\n%s\n"

		"void main(void)"
		"{"
			"fragColour = vec4(composite_sample(texID, inputPositionsVarying[5], iInputPositionVarying, phaseAndAmplitudeVarying.x, phaseAndAmplitudeVarying.y));"
		"}"
	, composite_sample);
	if(!composite_shader) free(composite_sample);

	std::unique_ptr<IntermediateShader> shader = make_shader(fragment_shader, true, true);
	free(fragment_shader);

	return shader;
}

std::unique_ptr<IntermediateShader> IntermediateShader::make_rgb_source_shader(const char *rgb_shader)
{
	char *fragment_shader;
	asprintf(&fragment_shader,
		"#version 150\n"

		"in vec2 inputPositionsVarying[11];"
		"in vec2 iInputPositionVarying;"
		"in vec2 phaseAndAmplitudeVarying;"

		"out vec3 fragColour;"

		"uniform usampler2D texID;"

		"\n%s\n"

		"void main(void)"
		"{"
			"fragColour = rgb_sample(texID, inputPositionsVarying[5], iInputPositionVarying);"
		"}"
	, rgb_shader);

	std::unique_ptr<IntermediateShader> shader = make_shader(fragment_shader, true, true);
	free(fragment_shader);

	return shader;
}

std::unique_ptr<IntermediateShader> IntermediateShader::make_chroma_luma_separation_shader()
{
	return make_shader(
		"#version 150\n"

		"in vec2 phaseAndAmplitudeVarying;"
		"in vec2 inputPositionsVarying[11];"
		"uniform vec4 weights[3];"

		"out vec3 fragColour;"

		"uniform sampler2D texID;"

		"void main(void)"
		"{"
			"vec4 samples[3] = vec4[]("
				"vec4("
					"texture(texID, inputPositionsVarying[0]).r,"
					"texture(texID, inputPositionsVarying[1]).r,"
					"texture(texID, inputPositionsVarying[2]).r,"
					"texture(texID, inputPositionsVarying[3]).r"
				"),"
				"vec4("
					"texture(texID, inputPositionsVarying[4]).r,"
					"texture(texID, inputPositionsVarying[5]).r,"
					"texture(texID, inputPositionsVarying[6]).r,"
					"texture(texID, inputPositionsVarying[7]).r"
				"),"
				"vec4("
					"texture(texID, inputPositionsVarying[8]).r,"
					"texture(texID, inputPositionsVarying[9]).r,"
					"texture(texID, inputPositionsVarying[10]).r,"
					"0.0"
				")"
			");"

			"float luminance = "
				"dot(vec3("
					"dot(samples[0], weights[0]),"
					"dot(samples[1], weights[1]),"
					"dot(samples[2], weights[2])"
				"), vec3(1.0));"

			"float chrominance = 0.5 * (samples[1].y - luminance) / phaseAndAmplitudeVarying.y;"
			"luminance /= (1.0 - phaseAndAmplitudeVarying.y);"

			"vec2 quadrature = vec2(cos(phaseAndAmplitudeVarying.x), -sin(phaseAndAmplitudeVarying.x));"
			"fragColour = vec3(luminance, vec2(0.5) + (chrominance * quadrature));"
		"}",false, false);
}

std::unique_ptr<IntermediateShader> IntermediateShader::make_chroma_filter_shader()
{
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

			"vec4 chromaChannel1[] = vec4[]("
				"vec4(samples[0].g, samples[1].g, samples[2].g, samples[3].g),"
				"vec4(samples[4].g, samples[5].g, samples[6].g, samples[7].g),"
				"vec4(samples[8].g, samples[9].g, samples[10].g, 0.0)"
			");"
			"vec4 chromaChannel2[] = vec4[]("
				"vec4(samples[0].b, samples[1].b, samples[2].b, samples[3].b),"
				"vec4(samples[4].b, samples[5].b, samples[6].b, samples[7].b),"
				"vec4(samples[8].b, samples[9].b, samples[10].b, 0.0)"
			");"

			"vec3 lumaChromaColour = vec3(samples[5].r,"
				"dot(vec3("
					"dot(chromaChannel1[0], weights[0]),"
					"dot(chromaChannel1[1], weights[1]),"
					"dot(chromaChannel1[2], weights[2])"
				"), vec3(1.0)),"
				"dot(vec3("
					"dot(chromaChannel2[0], weights[0]),"
					"dot(chromaChannel2[1], weights[1]),"
					"dot(chromaChannel2[2], weights[2])"
				"), vec3(1.0))"
			");"

			"vec3 lumaChromaColourInRange = (lumaChromaColour - vec3(0.0, 0.5, 0.5)) * vec3(1.0, 4.0, 4.0);"
			"fragColour = lumaChromaToRGB * lumaChromaColourInRange;"
		"}", false, false);
}

std::unique_ptr<IntermediateShader> IntermediateShader::make_luma_filter_shader()
{
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

			"vec4 luminance[] = vec4[]("
				"vec4(samples[0].r, samples[1].r, samples[2].r, samples[3].r),"
				"vec4(samples[4].r, samples[5].r, samples[6].r, samples[7].r),"
				"vec4(samples[8].r, samples[9].r, samples[10].r, 0.0)"
			");"

			"fragColour = vec3("
				"dot(vec3("
					"dot(luminance[0], weights[0]),"
					"dot(luminance[1], weights[1]),"
					"dot(luminance[2], weights[2])"
				"), vec3(1.0)),"
				"samples[5].gb"
			");"
		"}", false, false);
}

std::unique_ptr<IntermediateShader> IntermediateShader::make_rgb_filter_shader()
{
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

void IntermediateShader::set_output_size(unsigned int output_width, unsigned int output_height)
{
	bind();
	glUniform2i(outputTextureSizeUniform, (GLint)output_width, (GLint)output_height);
}

void IntermediateShader::set_source_texture_unit(GLenum unit)
{
	bind();
	glUniform1i(texIDUniform, (GLint)(unit - GL_TEXTURE0));
}

void IntermediateShader::set_filter_coefficients(float sampling_rate, float cutoff_frequency)
{
	bind();

	// The process below: the source texture will have bilinear filtering enabled; so by
	// sampling at non-integral offsets from the centre the shader can get a weighted sum
	// of two source pixels, then scale that once, to do two taps per sample. However
	// that works only if the two coefficients being joined have the same sign. So the
	// number of usable taps is between 11 and 21 depending on the values that come out.
	// Perform a linear search for the highest number of taps we can use with 11 samples.
	float weights[12];
	float offsets[5];
	unsigned int taps = 21;
	while(1)
	{
		float coefficients[21];
		SignalProcessing::FIRFilter luminance_filter(taps, sampling_rate, 0.0f, cutoff_frequency, SignalProcessing::FIRFilter::DefaultAttenuation);
		luminance_filter.get_coefficients(coefficients);

		int sample = 0;
		int c = 0;
		memset(weights, 0, sizeof(float)*12);
		memset(offsets, 0, sizeof(float)*5);

		int halfSize = (taps >> 1);
		while(c < halfSize && sample < 5)
		{
			offsets[sample] = (float)(halfSize - c);
			if((coefficients[c] < 0.0f) == (coefficients[c+1] < 0.0f) && c+1 < (taps >> 1))
			{
				weights[sample] = coefficients[c] + coefficients[c+1];
				offsets[sample] -= (coefficients[c+1] / weights[sample]);
				c += 2;
			}
			else
			{
				weights[sample] = coefficients[c];
				c++;
			}
			sample ++;
		}
		if(c == halfSize)	// i.e. we finished combining inputs before we ran out of space
		{
			weights[sample] = coefficients[c];
			for(int c = 0; c < sample; c++)
			{
				weights[sample+c+1] = weights[sample-c-1];
			}
			break;
		}
		taps -= 2;
	}

	glUniform4fv(weightsUniform, 3, weights);
	glUniform1fv(offsetsUniform, 5, offsets);
}

void IntermediateShader::set_separation_frequency(float sampling_rate, float colour_burst_frequency)
{
	// TODO: apply separately-formed filters for luminance and chrominance
	set_filter_coefficients(sampling_rate, colour_burst_frequency);
}

void IntermediateShader::set_phase_cycles_per_sample(float phase_cycles_per_sample, bool extend_runs_to_full_cycle)
{
	bind();
	glUniform1f(phaseCyclesPerTickUniform, phase_cycles_per_sample);
	glUniform1f(extensionUniform, extend_runs_to_full_cycle ? ceilf(1.0f / phase_cycles_per_sample) : 0.0f);
}

void IntermediateShader::set_colour_conversion_matrices(float *fromRGB, float *toRGB)
{
	bind();
	glUniformMatrix3fv(lumaChromaToRGBUniform, 1, GL_FALSE, toRGB);
	glUniformMatrix3fv(rgbToLumaChromaUniform, 1, GL_FALSE, fromRGB);
}
