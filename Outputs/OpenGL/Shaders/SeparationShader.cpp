//
//  SeparationShader.cpp
//  Clock Signal Kiosk
//
//  Created by Thomas Harte on 03/02/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include "SeparationShader.hpp"

#include "CommonAtrributes.hpp"
#include "DirtyZone.hpp"
#include "Outputs/ScanTargets/FilterGenerator.hpp"

#include <cstring>

namespace {

constexpr char vertex_shader[] = R"glsl(

uniform mediump float samplesPerLine;
uniform mediump vec2 bufferSize;

in mediump float zoneBegin;
in mediump float zoneEnd;

out mediump vec2 coordinates[31];

void main(void) {
	float lateral = float(gl_VertexID & 1);
	float longitudinal = float((gl_VertexID & 2) >> 1);

	float sampleY = mix(zoneBegin, zoneEnd, longitudinal);
	float centreX = lateral * samplesPerLine;

	coordinates[0] = vec2(centreX - 15.0, sampleY) / bufferSize;
	coordinates[1] = vec2(centreX - 14.0, sampleY) / bufferSize;
	coordinates[2] = vec2(centreX - 13.0, sampleY) / bufferSize;
	coordinates[3] = vec2(centreX - 12.0, sampleY) / bufferSize;
	coordinates[4] = vec2(centreX - 11.0, sampleY) / bufferSize;
	coordinates[5] = vec2(centreX - 10.0, sampleY) / bufferSize;
	coordinates[6] = vec2(centreX - 9.0, sampleY) / bufferSize;
	coordinates[7] = vec2(centreX - 8.0, sampleY) / bufferSize;
	coordinates[8] = vec2(centreX - 7.0, sampleY) / bufferSize;
	coordinates[9] = vec2(centreX - 6.0, sampleY) / bufferSize;
	coordinates[10] = vec2(centreX - 5.0, sampleY) / bufferSize;
	coordinates[11] = vec2(centreX - 4.0, sampleY) / bufferSize;
	coordinates[12] = vec2(centreX - 3.0, sampleY) / bufferSize;
	coordinates[13] = vec2(centreX - 2.0, sampleY) / bufferSize;
	coordinates[14] = vec2(centreX - 1.0, sampleY) / bufferSize;
	coordinates[15] = vec2(centreX + 0.0, sampleY) / bufferSize;
	coordinates[16] = vec2(centreX + 1.0, sampleY) / bufferSize;
	coordinates[17] = vec2(centreX + 2.0, sampleY) / bufferSize;
	coordinates[18] = vec2(centreX + 3.0, sampleY) / bufferSize;
	coordinates[19] = vec2(centreX + 4.0, sampleY) / bufferSize;
	coordinates[20] = vec2(centreX + 5.0, sampleY) / bufferSize;
	coordinates[21] = vec2(centreX + 6.0, sampleY) / bufferSize;
	coordinates[22] = vec2(centreX + 7.0, sampleY) / bufferSize;
	coordinates[23] = vec2(centreX + 8.0, sampleY) / bufferSize;
	coordinates[24] = vec2(centreX + 9.0, sampleY) / bufferSize;
	coordinates[25] = vec2(centreX + 10.0, sampleY) / bufferSize;
	coordinates[26] = vec2(centreX + 11.0, sampleY) / bufferSize;
	coordinates[27] = vec2(centreX + 12.0, sampleY) / bufferSize;
	coordinates[28] = vec2(centreX + 13.0, sampleY) / bufferSize;
	coordinates[29] = vec2(centreX + 14.0, sampleY) / bufferSize;
	coordinates[30] = vec2(centreX + 15.0, sampleY) / bufferSize;

	gl_Position = vec4(
		(vec2(centreX, sampleY) / bufferSize - vec2(0.5)) * vec2(2.0),
		0.0,
		1.0
	);
}

)glsl";

constexpr char fragment_shader[] = R"glsl(

uniform lowp sampler2D source;
uniform lowp vec2 filterCoefficients[31];

in mediump vec2 coordinates[31];

out lowp vec4 outputColour;

void main(void) {
	vec4 centre = texture(source, coordinates[15]);

	vec2 channels =
		filterCoefficients[0] * texture(source, coordinates[0]).x +
		filterCoefficients[1] * texture(source, coordinates[1]).x +
		filterCoefficients[2] * texture(source, coordinates[2]).x +
		filterCoefficients[3] * texture(source, coordinates[3]).x +
		filterCoefficients[4] * texture(source, coordinates[4]).x +
		filterCoefficients[5] * texture(source, coordinates[5]).x +
		filterCoefficients[6] * texture(source, coordinates[6]).x +
		filterCoefficients[7] * texture(source, coordinates[7]).x +
		filterCoefficients[8] * texture(source, coordinates[8]).x +
		filterCoefficients[9] * texture(source, coordinates[9]).x +
		filterCoefficients[10] * texture(source, coordinates[10]).x +
		filterCoefficients[11] * texture(source, coordinates[11]).x +
		filterCoefficients[12] * texture(source, coordinates[12]).x +
		filterCoefficients[13] * texture(source, coordinates[13]).x +
		filterCoefficients[14] * texture(source, coordinates[14]).x +
		filterCoefficients[15] * centre.x +
		filterCoefficients[16] * texture(source, coordinates[16]).x +
		filterCoefficients[17] * texture(source, coordinates[17]).x +
		filterCoefficients[18] * texture(source, coordinates[18]).x +
		filterCoefficients[19] * texture(source, coordinates[19]).x +
		filterCoefficients[20] * texture(source, coordinates[20]).x +
		filterCoefficients[21] * texture(source, coordinates[21]).x +
		filterCoefficients[22] * texture(source, coordinates[22]).x +
		filterCoefficients[23] * texture(source, coordinates[23]).x +
		filterCoefficients[24] * texture(source, coordinates[24]).x +
		filterCoefficients[25] * texture(source, coordinates[25]).x +
		filterCoefficients[26] * texture(source, coordinates[26]).x +
		filterCoefficients[27] * texture(source, coordinates[27]).x +
		filterCoefficients[28] * texture(source, coordinates[28]).x +
		filterCoefficients[29] * texture(source, coordinates[29]).x +
		filterCoefficients[30] * texture(source, coordinates[30]).x;

	outputColour = vec4(
		channels.x,
		channels.y * centre.yz,
		1.0
	);
}

)glsl";

}


using namespace Outputs::Display;

OpenGL::Shader OpenGL::separation_shader(
	const OpenGL::API api,
	const float per_line_subcarrier_frequency,
	const int samples_per_line,
	const int buffer_width,
	const int buffer_height,
	const VertexArray &vertex_array,
	const GLenum source_texture_unit
) {
	auto shader = OpenGL::Shader(
		api,
		vertex_shader,
		fragment_shader,
		dirty_zone_attributes()
	);

	//
	// Enable vertex attributes.
	//
	DirtyZone zone;
	vertex_array.bind_all();
	const auto enable = [&](const std::string &name, uint16_t &element) {
		shader.enable_vertex_attribute_with_pointer(
			name,
			1,
			GL_UNSIGNED_SHORT,
			GL_FALSE,
			sizeof(DirtyZone),
			reinterpret_cast<void *>((reinterpret_cast<uint8_t *>(&element) - reinterpret_cast<uint8_t *>(&zone))),
			1
		);
	};
	enable("zoneBegin", zone.begin);
	enable("zoneEnd", zone.end);

	//
	// Set uniforms.
	//
	shader.set_uniform("samplesPerLine", float(samples_per_line));
	shader.set_uniform("bufferSize", float(buffer_width), float(buffer_height));
	shader.set_uniform("source", GLint(source_texture_unit - GL_TEXTURE0));

	// Zip and provide the filter coefficients.
	const auto filter = FilterGenerator(
		samples_per_line,
		per_line_subcarrier_frequency,
		FilterGenerator::DecodingPath::Composite
	).separation_filter();
	struct FilterElement {
		float x, y;
	};

	static_assert(FilterGenerator::MaxKernelSize <= 31);
	FilterElement elements[31]{};
	filter.luma.copy_to(std::begin(elements), std::end(elements),
		[](const auto iterator, const float coefficient) {
			iterator->x = coefficient;
		}
	);
	filter.chroma.copy_to(std::begin(elements), std::end(elements),
		[](const auto iterator, const float coefficient) {
			iterator->y = coefficient;
		}
	);

	float packaged_elements[31 * 2];
	static_assert(sizeof(packaged_elements) == sizeof(elements));
	std::memcpy(packaged_elements, elements, sizeof(elements));
	shader.set_uniform("filterCoefficients", 2, 31, packaged_elements);

	return shader;
}
