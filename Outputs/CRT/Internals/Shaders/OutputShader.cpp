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
		{nullptr}
	};
}

std::unique_ptr<OutputShader> OutputShader::make_shader(const char *fragment_methods, const char *colour_expression, bool use_usampler)
{
	const char *sampler_type = use_usampler ? "usampler2D" : "sampler2D";

	char *vertex_shader;
	asprintf(&vertex_shader,
		"#version 150\n"

		"in vec2 horizontal;"
		"in vec2 vertical;"

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
			"float lateral = float(gl_VertexID & 1);"
			"float longitudinal = float((gl_VertexID & 2) >> 1);"
			"float x = mix(horizontal.x, horizontal.y, longitudinal);"

			"lateralVarying = lateral - 0.5;"

			"vec2 vSrcCoordinates = vec2(x, vertical.y);"
			"ivec2 textureSize = textureSize(texID, 0);"
			"iSrcCoordinatesVarying = vSrcCoordinates;"
			"srcCoordinatesVarying = vec2(vSrcCoordinates.x / textureSize.x, (vSrcCoordinates.y + 0.5) / textureSize.y);"

			"vec2 vPosition = vec2(x, vertical.x);"
			"vec2 floatingPosition = (vPosition / positionConversion) + lateral * scanNormal;"
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

	return result;
}

void OutputShader::set_output_size(unsigned int output_width, unsigned int output_height, Outputs::CRT::Rect visible_area)
{
	GLfloat outputAspectRatioMultiplier = ((float)output_width / (float)output_height) / (4.0f / 3.0f);

	GLfloat bonusWidth = (outputAspectRatioMultiplier - 1.0f) * visible_area.size.width;
	visible_area.origin.x -= bonusWidth * 0.5f * visible_area.size.width;
	visible_area.size.width *= outputAspectRatioMultiplier;

	set_uniform("boundsOrigin", (GLfloat)visible_area.origin.x, (GLfloat)visible_area.origin.y);
	set_uniform("boundsSize", (GLfloat)visible_area.size.width, (GLfloat)visible_area.size.height);
}

void OutputShader::set_source_texture_unit(GLenum unit)
{
	set_uniform("texID", (GLint)(unit - GL_TEXTURE0));
}

void OutputShader::set_timing(unsigned int height_of_display, unsigned int cycles_per_line, unsigned int horizontal_scan_period, unsigned int vertical_scan_period, unsigned int vertical_period_divider)
{
	GLfloat scan_angle = atan2f(1.0f / (float)height_of_display, 1.0f);
	GLfloat scan_normal[] = { -sinf(scan_angle), cosf(scan_angle)};
	GLfloat multiplier = (float)cycles_per_line / ((float)height_of_display * (float)horizontal_scan_period);
	scan_normal[0] *= multiplier;
	scan_normal[1] *= multiplier;

	set_uniform("scanNormal", scan_normal[0], scan_normal[1]);
	set_uniform("positionConversion", (GLfloat)horizontal_scan_period, (GLfloat)vertical_scan_period / (GLfloat)vertical_period_divider);
}
