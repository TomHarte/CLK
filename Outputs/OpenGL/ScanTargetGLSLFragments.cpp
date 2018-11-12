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

			"in vec2 startPoint;"
			"in vec2 endPoint;";
	}
}

std::string ScanTarget::glsl_default_vertex_shader(ShaderType type) {
	switch(type) {
		case ShaderType::Scan:
		return
			"void main(void) {"
				"float lateral = float(gl_VertexID & 1);"
				"float longitudinal = float((gl_VertexID & 2) >> 1);"

				"vec2 centrePoint = mix(startPoint, endPoint, lateral) / scale;"
				"vec2 height = normalize(endPoint - startPoint).yx * (longitudinal - 0.5) * rowHeight;"
				"vec2 eyePosition = vec2(1.0) - 2.0 * (centrePoint + height);"
				"gl_Position = vec4(eyePosition, 0.0, 1.0);"
			"}";

		case ShaderType::Line:
		return "";
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
		break;
	}
}
