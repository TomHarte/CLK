//
//  LineOutputShader.cpp
//  Clock Signal Kiosk
//
//  Created by Thomas Harte on 04/02/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include "LineOutputShader.hpp"

#include "CommonAtrributes.hpp"
#include "Outputs/ScanTargets/BufferingScanTarget.hpp"

namespace {

constexpr char vertex_shader[] = R"glsl(

uniform vec2 sourceSize;
uniform vec2 positionScale;
uniform float lineHeight;
uniform mat3 scale;

in vec2 lineEndpoint0Position;
in float lineEndpoint0CyclesSinceRetrace;

in vec2 lineEndpoint1Position;
in float lineEndpoint1CyclesSinceRetrace;

in float lineLine;

out vec2 coordinate;

void main(void) {
	float lateral = float(gl_VertexID & 1);
	float longitudinal = float((gl_VertexID & 2) >> 1);

	coordinate = vec2(
		mix(
			lineEndpoint0CyclesSinceRetrace,
			lineEndpoint1CyclesSinceRetrace,
			lateral
		),
		sourceSize.y - lineLine - 0.5
	) / sourceSize;

	vec2 tangent = normalize(lineEndpoint1Position - lineEndpoint0Position);
	vec2 normal = vec2(tangent.y, -tangent.x);

	vec2 centre =
		mix(
			lineEndpoint0Position,
			lineEndpoint1Position,
			lateral
		) / positionScale;
	gl_Position =
		vec4(
			(scale * vec3(centre + (longitudinal - 0.5) * normal * lineHeight, 1.0)).xy,
			0.0,
			1.0
		) ;
}

)glsl";

constexpr char fragment_shader[] = R"glsl(

uniform sampler2D source;
uniform float alpha;
in vec2 coordinate;

out vec4 outputColour;

void main(void) {
	outputColour = texture(source, coordinate) * vec4(1.0, 1.0, 1.0, alpha);
}

)glsl";

}

using namespace Outputs::Display;

OpenGL::LineOutputShader::LineOutputShader(
	const API api,
	const int source_width,
	const int source_height,
	const float cycle_multiplier,
	const int expected_vertical_lines,
	const int scale_x,
	const int scale_y,
	const float alpha,
	const VertexArray &vertex_array,
	const GLenum source_texture_unit
) {
	shader_ = OpenGL::Shader(
		api,
		vertex_shader,
		fragment_shader,
		line_attributes()
	);

	BufferingScanTarget::Line line;
	vertex_array.bind_all();
	const auto enable = [&](const std::string &name, uint16_t &element, const GLint size) {
		shader_.enable_vertex_attribute_with_pointer(
			name,
			size,
			GL_UNSIGNED_SHORT,
			GL_FALSE,
			sizeof(line),
			reinterpret_cast<void *>((reinterpret_cast<uint8_t *>(&element) - reinterpret_cast<uint8_t *>(&line))),
			1
		);
	};
	enable("lineEndpoint0Position", line.end_points[0].x, 2);
	enable("lineEndpoint1Position", line.end_points[1].x, 2);
	enable("lineEndpoint0CyclesSinceRetrace", line.end_points[0].cycles_since_end_of_horizontal_retrace, 1);
	enable("lineEndpoint1CyclesSinceRetrace", line.end_points[1].cycles_since_end_of_horizontal_retrace, 1);
	enable("lineLine", line.line, 1);

	shader_.set_uniform("lineHeight", 1.05f / GLfloat(expected_vertical_lines));
	shader_.set_uniform("positionScale", GLfloat(scale_x), GLfloat(scale_y));
	shader_.set_uniform("sourceSize", GLfloat(source_width) / cycle_multiplier, GLfloat(source_height));
	shader_.set_uniform("source", GLint(source_texture_unit - GL_TEXTURE0));
	shader_.set_uniform("alpha", GLfloat(alpha));
}

void OpenGL::LineOutputShader::set_aspect_ratio_transformation(const std::array<float, 9> &transform) {
	shader_.set_uniform_matrix("scale", 3, false, transform.data());
}

void OpenGL::LineOutputShader::bind() {
	shader_.bind();
}
