//
//  OutputShader.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 27/04/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "OutputShader.hpp"

#include <cmath>
#include <sstream>

using namespace OpenGL;

std::string OutputShader::get_input_name(Input input) {
	switch(input) {
		case Input::Horizontal:	return "horizontal";
		case Input::Vertical:	return "vertical";
	}
}

std::unique_ptr<OutputShader> OutputShader::make_shader(const char *fragment_methods, const char *colour_expression, bool use_usampler) {
	const std::string sampler_type = use_usampler ? "usampler2D" : "sampler2D";

	std::ostringstream vertex_shader;
	vertex_shader <<
		"#version 150\n"

		"in vec2 " << get_input_name(Input::Horizontal) << ";"
		"in vec2 " << get_input_name(Input::Vertical) << ";"

		"uniform vec2 boundsOrigin;"
		"uniform vec2 boundsSize;"
		"uniform vec2 positionConversion;"
		"uniform vec2 scanNormal;"
		"uniform " << sampler_type << " texID;"
		"uniform float inputScaler;"
		"uniform int textureHeightDivisor;"

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
			"ivec2 textureSize = textureSize(texID, 0) * ivec2(1, textureHeightDivisor);"
			"iSrcCoordinatesVarying = vSrcCoordinates;"
			"srcCoordinatesVarying = vec2(inputScaler * vSrcCoordinates.x / textureSize.x, (vSrcCoordinates.y + 0.5) / textureSize.y);"
			"srcCoordinatesVarying.x = srcCoordinatesVarying.x - mod(srcCoordinatesVarying.x, 1.0 / textureSize.x);"

			"vec2 vPosition = vec2(x, vertical.x);"
			"vec2 floatingPosition = (vPosition / positionConversion) + lateral * scanNormal;"
			"vec2 mappedPosition = (floatingPosition - boundsOrigin) / boundsSize;"
			"gl_Position = vec4(mappedPosition.x * 2.0 - 1.0, 1.0 - mappedPosition.y * 2.0, 0.0, 1.0);"
		"}";

	std::ostringstream fragment_shader;
	fragment_shader <<
		"#version 150\n"

		"in float lateralVarying;"
		"in vec2 srcCoordinatesVarying;"
		"in vec2 iSrcCoordinatesVarying;"

		"out vec4 fragColour;"

		"uniform " << sampler_type << " texID;"
		"uniform float gamma;"

		<< fragment_methods <<

		"void main(void)"
		"{"
			"fragColour = vec4(pow(" << colour_expression << ", vec3(gamma)), 0.5);"//*cos(lateralVarying)
		"}";

	return std::unique_ptr<OutputShader>(new OutputShader(vertex_shader.str(), fragment_shader.str(), {
		{get_input_name(Input::Horizontal), 0},
		{get_input_name(Input::Vertical), 1}
	}));
}

void OutputShader::set_output_size(unsigned int output_width, unsigned int output_height, Outputs::CRT::Rect visible_area) {
	GLfloat outputAspectRatioMultiplier = (static_cast<float>(output_width) / static_cast<float>(output_height)) / (4.0f / 3.0f);

	GLfloat bonusWidth = (outputAspectRatioMultiplier - 1.0f) * visible_area.size.width;
	visible_area.origin.x -= bonusWidth * 0.5f * visible_area.size.width;
	visible_area.size.width *= outputAspectRatioMultiplier;

	set_uniform("boundsOrigin", (GLfloat)visible_area.origin.x, (GLfloat)visible_area.origin.y);
	set_uniform("boundsSize", (GLfloat)visible_area.size.width, (GLfloat)visible_area.size.height);
}

void OutputShader::set_source_texture_unit(GLenum unit) {
	set_uniform("texID", (GLint)(unit - GL_TEXTURE0));
}

void OutputShader::set_timing(unsigned int height_of_display, unsigned int cycles_per_line, unsigned int horizontal_scan_period, unsigned int vertical_scan_period, unsigned int vertical_period_divider) {
	GLfloat scan_angle = atan2f(1.0f / static_cast<float>(height_of_display), 1.0f);
	GLfloat scan_normal[] = { -sinf(scan_angle), cosf(scan_angle)};
	GLfloat multiplier = static_cast<float>(cycles_per_line) / (static_cast<float>(height_of_display) * static_cast<float>(horizontal_scan_period));
	scan_normal[0] *= multiplier;
	scan_normal[1] *= multiplier;

	set_uniform("scanNormal", scan_normal[0], scan_normal[1]);
	set_uniform("positionConversion", (GLfloat)horizontal_scan_period, (GLfloat)vertical_scan_period / (GLfloat)vertical_period_divider);
}

void OutputShader::set_gamma_ratio(float ratio) {
	set_uniform("gamma", ratio);
}

void OutputShader::set_input_width_scaler(float input_scaler) {
	set_uniform("inputScaler", input_scaler);
}

void OutputShader::set_origin_is_double_height(bool is_double_height) {
	set_uniform("textureHeightDivisor", is_double_height ? 2 : 1);
}
