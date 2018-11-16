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
			"uniform float rowHeight;"
			"uniform mat3 lumaChromaToRGB;"
			"uniform mat3 rgbToLumaChroma;"
			"uniform float processingWidth;"

			"in vec2 startPoint;"
			"in float startDataX;"
			"in float startCompositeAngle;"

			"in vec2 endPoint;"
			"in float endDataX;"
			"in float endCompositeAngle;"

			"in float dataY;"
			"in float lineY;";

		case ShaderType::Line:
		return
			"#version 150\n"

			"uniform vec2 scale;"
			"uniform float rowHeight;"
			"uniform float processingWidth;"

			"in vec2 startPoint;"
			"in vec2 endPoint;"

			"in float lineY;";
	}
}

std::string ScanTarget::glsl_default_vertex_shader(ShaderType type) {
	switch(type) {
		case ShaderType::Scan:
		return
			"out vec2 textureCoordinate;"
			"uniform usampler2D textureName;"

			"void main(void) {"
				"float lateral = float(gl_VertexID & 1);"
				"float longitudinal = float((gl_VertexID & 2) >> 1);"

				"textureCoordinate = vec2(mix(startDataX, endDataX, lateral), dataY) / textureSize(textureName, 0);"

				"vec2 eyePosition = vec2(mix(startPoint.x, endPoint.x, lateral) * processingWidth, lineY + longitudinal) / vec2(scale.x, 2048.0);"
				"gl_Position = vec4(eyePosition*2 - vec2(1.0), 0.0, 1.0);"
			"}";

		case ShaderType::Line:
		return
			"out vec2 textureCoordinate;"
			"uniform sampler2D textureName;"

			"void main(void) {"
				"float lateral = float(gl_VertexID & 1);"
				"float longitudinal = float((gl_VertexID & 2) >> 1);"

				"textureCoordinate = vec2(lateral * processingWidth, lineY) / vec2(1.0, textureSize(textureName, 0).y);"

				"vec2 centrePoint = mix(startPoint, endPoint, lateral) / scale;"
				"vec2 height = normalize(endPoint - startPoint).yx * (longitudinal - 0.5) * rowHeight;"
				"vec2 eyePosition = vec2(-1.0, 1.0) + vec2(2.0, -2.0) * (centrePoint + height);"
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

std::unique_ptr<Shader> ScanTarget::input_shader(InputDataType input_data_type, OutputType output_type) {
	std::string fragment_shader =
		"#version 150\n"

		"out vec4 fragColour;"
		"in vec2 textureCoordinate;";

	switch(input_data_type) {
		case InputDataType::Luminance1:
			fragment_shader +=
				"uniform usampler2D textureName;"
				"void main(void) {";

			switch(output_type) {
				case OutputType::RGB:
					fragment_shader += "fragColour = vec4(texture(textureName, textureCoordinate).rrr, 1.0);";
				break;
				default:
					fragment_shader += "fragColour = vec4(texture(textureName, textureCoordinate).r, 0.0, 0.0, 1.0);";
				break;
			}

			fragment_shader += "}";
		break;

		case InputDataType::Luminance8:
		break;

//	SVideo,
//	CompositeColour,
//	CompositeMonochrome

		case InputDataType::Phase8Luminance8:
		return nullptr;
//			fragment_shader +=
//				"uniform sampler2D textureName;"
//				"void main(void) {";
//
//			switch(output_type) {
//				default: return nullptr;
//
//				case OutputType::SVideo:
//				break;
////	CompositeColour,
////	CompositeMonochrome
//			}
//
//			fragment_shader += "}";
//		break;

		case InputDataType::Red1Green1Blue1:
			// TODO: write encoding functions for RGB -> composite/s-video.
			fragment_shader +=
				"uniform usampler2D textureName;"
				"void main(void) {"
					"uint textureValue = texture(textureName, textureCoordinate).r;"
					"fragColour = vec4(uvec3(textureValue) & uvec3(4u, 2u, 1u), 1.0);"
				"}";
		break;

		case InputDataType::Red2Green2Blue2:
		break;

		case InputDataType::Red4Green4Blue4:
		break;

		case InputDataType::Red8Green8Blue8:
			fragment_shader +=
				"uniform usampler2D textureName;"
				"void main(void) {"
					"fragColour = vec4(texture(textureName, textureCoordinate).rgb / vec3(255.0), 1.0);"
				"}";
		break;
	}

	return std::unique_ptr<Shader>(new Shader(
		glsl_globals(ShaderType::Scan) + glsl_default_vertex_shader(ShaderType::Scan),
		fragment_shader
	));
}
