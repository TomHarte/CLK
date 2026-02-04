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

uniform mediump vec2 sourceSize;
uniform highp vec2 positionScale;
uniform mediump float lineHeight;

// TODO: programmable crop should affect scaling via uniforms.

in highp vec2 lineEndpoint0Position;
in highp float lineEndpoint0CyclesSinceRetrace;

in highp vec2 lineEndpoint1Position;
in highp float lineEndpoint1CyclesSinceRetrace;

in highp float lineLine;

out mediump vec2 coordinate;

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
			(centre + (longitudinal - 0.5) * normal * lineHeight) * vec2(2.0, -2.0) + vec2(-1.0, 1.0),
			0.0,
			1.0
		) ;
}

)glsl";

constexpr char fragment_shader[] = R"glsl(

uniform lowp sampler2D source;
in mediump vec2 coordinate;

out lowp vec4 outputColour;

void main(void) {
	outputColour = texture(source, coordinate);
}

)glsl";

}

using namespace Outputs::Display;

OpenGL::Shader OpenGL::line_output_shader(
	const API api,
	const int source_width,
	const int source_height,
	const float cycle_multiplier,
	const int expected_vertical_lines,
	const int scale_x,
	const int scale_y,
	const VertexArray &vertex_array,
	const GLenum source_texture_unit
) {
	auto shader = OpenGL::Shader(
		api,
		vertex_shader,
		fragment_shader,
		line_attributes()
	);

	BufferingScanTarget::Line line;
	vertex_array.bind_all();
	const auto enable = [&](const std::string &name, uint16_t &element, const GLint size) {
		shader.enable_vertex_attribute_with_pointer(
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

	shader.set_uniform("lineHeight", 1.05f / float(expected_vertical_lines));
	shader.set_uniform("positionScale", float(scale_x), float(scale_y));
	shader.set_uniform("sourceSize", float(source_width) / cycle_multiplier, float(source_height));
	shader.set_uniform("source", GLint(source_texture_unit - GL_TEXTURE0));

	return shader;
}
