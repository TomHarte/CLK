//
//  OutputShader.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 27/04/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "OutputShader.hpp"

#include <stdlib.h>
#include <math.h>

using namespace OpenGL;

namespace {
	const OpenGL::Shader::AttributeBinding bindings[] =
	{
		{"position", 0},
		{"srcCoordinates", 1},
		{"lateral", 2},
		{nullptr}
	};
}

std::unique_ptr<OutputShader> OutputShader::make_shader(const char *fragment_methods, const char *colour_expression, bool use_usampler)
{
	const char *sampler_type = use_usampler ? "usampler2D" : "sampler2D";

	char *vertex_shader;
	asprintf(&vertex_shader,
		"#version 150\n"

		"in vec2 position;"
		"in vec2 srcCoordinates;"
//		"in float lateral;"

		"uniform vec2 boundsOrigin;"
		"uniform vec2 boundsSize;"
		"uniform vec2 positionConversion;"
		"uniform vec2 scanNormal;"
		"uniform %s texID;"

		"out float lateralVarying;"
		"out vec2 srcCoordinatesVarying;"
		"out vec2 iSrcCoordinatesVarying;"

		"void main(void)"
		"{"
			"float laterals[] = float[](0, 0, 1, 0, 1, 1);"
			"float lateral = laterals[gl_VertexID %% 6];"
			"lateralVarying = lateral - 0.5;"

			"ivec2 textureSize = textureSize(texID, 0);"
			"iSrcCoordinatesVarying = srcCoordinates;"
			"srcCoordinatesVarying = vec2(srcCoordinates.x / textureSize.x, (srcCoordinates.y + 0.5) / textureSize.y);"

			"vec2 floatingPosition = (position / positionConversion) + lateral * scanNormal;"
			"vec2 mappedPosition = (floatingPosition - boundsOrigin) / boundsSize;"
			"gl_Position = vec4(mappedPosition.x * 2.0 - 1.0, 1.0 - mappedPosition.y * 2.0, 0.0, 1.0);"
		"}", sampler_type);

	char *fragment_shader;
	asprintf(&fragment_shader,
		"#version 150\n"

		"in float lateralVarying;"
		"in vec2 srcCoordinatesVarying;"
		"in vec2 iSrcCoordinatesVarying;"

		"out vec4 fragColour;"

		"uniform %s texID;"

		"\n%s\n"

		"void main(void)"
		"{"
			"fragColour = vec4(%s, 0.5*cos(lateralVarying));"
		"}",
	sampler_type, fragment_methods, colour_expression);

	std::unique_ptr<OutputShader> result = std::unique_ptr<OutputShader>(new OutputShader(vertex_shader, fragment_shader, bindings));
	free(vertex_shader);
	free(fragment_shader);

	result->boundsSizeUniform			= result->get_uniform_location("boundsSize");
	result->boundsOriginUniform			= result->get_uniform_location("boundsOrigin");
	result->texIDUniform				= result->get_uniform_location("texID");
	result->scanNormalUniform			= result->get_uniform_location("scanNormal");
	result->positionConversionUniform	= result->get_uniform_location("positionConversion");

	return result;
}

void OutputShader::set_output_size(unsigned int output_width, unsigned int output_height, Outputs::CRT::Rect visible_area)
{
	bind();

	GLfloat outputAspectRatioMultiplier = ((float)output_width / (float)output_height) / (4.0f / 3.0f);

	GLfloat bonusWidth = (outputAspectRatioMultiplier - 1.0f) * visible_area.size.width;
	visible_area.origin.x -= bonusWidth * 0.5f * visible_area.size.width;
	visible_area.size.width *= outputAspectRatioMultiplier;

	glUniform2f(boundsOriginUniform, (GLfloat)visible_area.origin.x, (GLfloat)visible_area.origin.y);
	glUniform2f(boundsSizeUniform, (GLfloat)visible_area.size.width, (GLfloat)visible_area.size.height);
}

void OutputShader::set_source_texture_unit(GLenum unit)
{
	bind();
	glUniform1i(texIDUniform, (GLint)(unit - GL_TEXTURE0));
}

void OutputShader::set_timing(unsigned int height_of_display, unsigned int cycles_per_line, unsigned int horizontal_scan_period, unsigned int vertical_scan_period, unsigned int vertical_period_divider)
{
	bind();

	float scan_angle = atan2f(1.0f / (float)height_of_display, 1.0f);
	float scan_normal[] = { -sinf(scan_angle), cosf(scan_angle)};
	float multiplier = (float)cycles_per_line / ((float)height_of_display * (float)horizontal_scan_period);
	scan_normal[0] *= multiplier;
	scan_normal[1] *= multiplier;

	glUniform2f(scanNormalUniform, scan_normal[0], scan_normal[1]);
	glUniform2f(positionConversionUniform, horizontal_scan_period, vertical_scan_period / (unsigned int)vertical_period_divider);
}
