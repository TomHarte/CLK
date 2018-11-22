//
//  ScanTargetVertexArrayAttributs.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 11/11/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#include "ScanTarget.hpp"

using namespace Outputs::Display::OpenGL;

std::string ScanTarget::glsl_globals(ShaderType type) {
	switch(type) {
		case ShaderType::Scan:
		return
			"#version 150\n"

			"uniform vec2 scale;"

			"uniform mat3 lumaChromaToRGB;"
			"uniform mat3 rgbToLumaChroma;"

			"uniform float rowHeight;"
			"uniform float processingWidth;"

			"in vec2 startPoint;"
			"in float startDataX;"
			"in float startCompositeAngle;"

			"in vec2 endPoint;"
			"in float endDataX;"
			"in float endCompositeAngle;"

			"in float dataY;"
			"in float lineY;"
			"in float compositeAmplitude;"

			"uniform usampler2D textureName;";

		case ShaderType::Line:
		return
			"#version 150\n"

			"uniform vec2 scale;"
			"uniform float rowHeight;"
			"uniform float processingWidth;"

			"in vec2 startPoint;"
			"in vec2 endPoint;"

			"in float lineY;"

			"uniform sampler2D textureName;"
			"uniform vec2 origin;"
			"uniform vec2 size;";
	}
}

std::string ScanTarget::glsl_default_vertex_shader(ShaderType type) {
	switch(type) {
		case ShaderType::Scan:
		return
			"out vec2 textureCoordinate;"
			"out float compositeAngle;"
			"out float compositeAmplitudeOut;"

			"void main(void) {"
				"float lateral = float(gl_VertexID & 1);"
				"float longitudinal = float((gl_VertexID & 2) >> 1);"

				"textureCoordinate = vec2(mix(startDataX, endDataX, lateral), dataY) / textureSize(textureName, 0);"
				"compositeAngle = (mix(startCompositeAngle, endCompositeAngle, lateral) / 32.0) * 3.141592654;"
				"compositeAmplitudeOut = compositeAmplitude / 255.0;"

				"vec2 eyePosition = vec2(mix(startPoint.x, endPoint.x, lateral) * processingWidth, lineY + longitudinal) / vec2(scale.x, 2048.0);"
				"gl_Position = vec4(eyePosition*2 - vec2(1.0), 0.0, 1.0);"
			"}";

		case ShaderType::Line:
		return
			"out vec2 textureCoordinate;"

			"void main(void) {"
				"float lateral = float(gl_VertexID & 1);"
				"float longitudinal = float((gl_VertexID & 2) >> 1);"

				"textureCoordinate = vec2(lateral * processingWidth, lineY + 0.5) / vec2(1.0, textureSize(textureName, 0).y);"

				"vec2 centrePoint = mix(startPoint, endPoint, lateral) / scale;"
				"vec2 height = normalize(endPoint - startPoint).yx * (longitudinal - 0.5) * rowHeight;"
				"vec2 eyePosition = vec2(-1.0, 1.0) + vec2(2.0, -2.0) * (((centrePoint + height) - origin) / size);"
				"gl_Position = vec4(eyePosition, 0.0, 1.0);"
			"}";
	}
}

void ScanTarget::enable_vertex_attributes(ShaderType type, Shader &target) {
	switch(type) {
		case ShaderType::Scan:
			for(int c = 0; c < 2; ++c) {
				const std::string prefix = c ? "end" : "start";

				target.enable_vertex_attribute_with_pointer(
					prefix + "Point",
					2, GL_UNSIGNED_SHORT, GL_FALSE,
					sizeof(Scan),
					reinterpret_cast<void *>(offsetof(Scan, scan.end_points[c].x)),
					1);
				target.enable_vertex_attribute_with_pointer(
					prefix + "DataX",
					1, GL_UNSIGNED_SHORT, GL_FALSE,
					sizeof(Scan),
					reinterpret_cast<void *>(offsetof(Scan, scan.end_points[c].data_offset)),
					1);
				target.enable_vertex_attribute_with_pointer(
					prefix + "CompositeAngle",
					1, GL_UNSIGNED_SHORT, GL_FALSE,
					sizeof(Scan),
					reinterpret_cast<void *>(offsetof(Scan, scan.end_points[c].composite_angle)),
					1);
			}

			target.enable_vertex_attribute_with_pointer(
				"dataY",
				1, GL_UNSIGNED_SHORT, GL_FALSE,
				sizeof(Scan),
				reinterpret_cast<void *>(offsetof(Scan, data_y)),
				1);
			target.enable_vertex_attribute_with_pointer(
				"lineY",
				1, GL_UNSIGNED_SHORT, GL_FALSE,
				sizeof(Scan),
				reinterpret_cast<void *>(offsetof(Scan, line)),
				1);
			target.enable_vertex_attribute_with_pointer(
				"compositeAmplitude",
				1, GL_UNSIGNED_BYTE, GL_FALSE,
				sizeof(Scan),
				reinterpret_cast<void *>(offsetof(Scan, scan.composite_amplitude)),
				1);
		break;

		case ShaderType::Line:
			for(int c = 0; c < 2; ++c) {
				const std::string prefix = c ? "end" : "start";

				target.enable_vertex_attribute_with_pointer(
					prefix + "Point",
					2, GL_UNSIGNED_SHORT, GL_FALSE,
					sizeof(Line),
					reinterpret_cast<void *>(offsetof(Line, end_points[c].x)),
					1);
			}

			target.enable_vertex_attribute_with_pointer(
				"lineY",
				1, GL_UNSIGNED_SHORT, GL_FALSE,
				sizeof(Line),
				reinterpret_cast<void *>(offsetof(Line, line)),
				1);
		break;
	}
}

std::unique_ptr<Shader> ScanTarget::input_shader(InputDataType input_data_type, DisplayType display_type) {
	std::string fragment_shader =
		"#version 150\n"

		"out vec4 fragColour;"
		"in vec2 textureCoordinate;"
		"in float compositeAngle;"
		"in float compositeAmplitudeOut;"

		"uniform mat3 lumaChromaToRGB;"
		"uniform mat3 rgbToLumaChroma;"
		"uniform usampler2D textureName;"

		"void main(void) {";

	DisplayType computed_display_type;
	switch(input_data_type) {
		case InputDataType::Luminance1:
			computed_display_type = DisplayType::CompositeMonochrome;
			fragment_shader += "fragColour = vec4(vec3(texture(textureName, textureCoordinate).r), 1.0);";
		break;

		case InputDataType::Luminance8:
			computed_display_type = DisplayType::CompositeMonochrome;
			fragment_shader += "fragColour = vec4(vec3(texture(textureName, textureCoordinate).r / 255.0), 1.0);";
		break;

		case InputDataType::Phase8Luminance8:
			computed_display_type = DisplayType::SVideo;
			fragment_shader += "fragColour = vec4(texture(textureName, textureCoordinate).rg / vec2(255.0), 0.0, 1.0);";
		break;

		case InputDataType::Red1Green1Blue1:
			computed_display_type = DisplayType::RGB;
			fragment_shader +=
				"uint textureValue = texture(textureName, textureCoordinate).r;"
				"fragColour = vec4(uvec3(textureValue) & uvec3(4u, 2u, 1u), 1.0);";
		break;

		case InputDataType::Red2Green2Blue2:
			computed_display_type = DisplayType::RGB;
			fragment_shader +=
				"uint textureValue = texture(textureName, textureCoordinate).r;"
				"fragColour = vec4(vec3(float((textureValue >> 4) & 3u), float((textureValue >> 2) & 3u), float(textureValue & 3u)) / 3.0, 1.0);";
		break;

		case InputDataType::Red4Green4Blue4:
			computed_display_type = DisplayType::RGB;
			fragment_shader +=
				"uvec2 textureValue = texture(textureName, textureCoordinate).rg;"
				"fragColour = vec4(float(textureValue.r) / 15.0, float(textureValue.g & 240u) / 240.0, float(textureValue.g & 15u) / 15.0, 1.0);";
		break;

		case InputDataType::Red8Green8Blue8:
			computed_display_type = DisplayType::RGB;
			fragment_shader += "fragColour = vec4(texture(textureName, textureCoordinate).rgb / vec3(255.0), 1.0);";
		break;
	}

	if(computed_display_type != display_type) {
		// If the input type is RGB but the output type isn't then
		// there'll definitely be an RGB to SVideo step.
		if(computed_display_type == DisplayType::RGB) {
			fragment_shader +=
				"vec3 composite_colour = rgbToLumaChroma * vec3(fragColour);"
				"vec2 quadrature = vec2(cos(compositeAngle), sin(compositeAngle));"
				"fragColour = vec4(composite_colour.r, 0.5 + dot(quadrature, composite_colour.gb)*0.5, 0.0, 1.0);";
		}

		// If the output type isn't SVideo, add an SVideo to composite step.
		if(computed_display_type != DisplayType::SVideo) {
			fragment_shader += "fragColour = vec4(vec3(mix(fragColour.r,fragColour.g, compositeAmplitudeOut)), 1.0);";
		}
	}

	return std::unique_ptr<Shader>(new Shader(
		glsl_globals(ShaderType::Scan) + glsl_default_vertex_shader(ShaderType::Scan),
		fragment_shader + "}"
	));
}
