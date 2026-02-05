//
//  KernelShaders.cpp
//  Clock Signal Kiosk
//
//  Created by Thomas Harte on 03/02/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include "KernelShaders.hpp"

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

constexpr char separation_fragment_shader[] = R"glsl(
#define KernelCentre 15

uniform lowp sampler2D source;
uniform lowp vec2 filterCoefficients[16];

in mediump vec2 coordinates[31];

out lowp vec4 outputColour;

void main(void) {
	vec4 centre = texture(source, coordinates[15]);
#define Sample(x) \
	texture(source, coordinates[x]).r * filterCoefficients[x > KernelCentre ? KernelCentre - (x - KernelCentre) : x]

	vec2 channels =
		Sample(0) +		Sample(1) +		Sample(2) +		Sample(3) +
		Sample(4) +		Sample(5) +		Sample(6) +		Sample(7) +
		Sample(8) +		Sample(9) +		Sample(10) +	Sample(11) +
		Sample(12) +	Sample(13) +	Sample(14) +
		filterCoefficients[15] * centre.x +
		Sample(16) +	Sample(17) +	Sample(18) +
		Sample(19) +	Sample(20) +	Sample(21) +	Sample(22) +
		Sample(23) +	Sample(24) +	Sample(25) +	Sample(26) +
		Sample(27) +	Sample(28) +	Sample(29) +	Sample(30);

#undef Sample

	lowp float colourAmplitude = centre.a;
	lowp float isColour = step(0.01, colourAmplitude);
	lowp float chromaScale = mix(1.0, colourAmplitude, isColour);
	lowp float lumaScale = mix(1.0, 1.0 - colourAmplitude * 2.0, isColour);
	outputColour = vec4(
		(channels.x - colourAmplitude) / lumaScale,
		isColour * channels.y * (centre.yz / chromaScale) + vec2(0.5),
		1.0
	);
}

)glsl";

constexpr char demodulation_fragment_shader[] = R"glsl(
#define KernelCentre 15

uniform lowp sampler2D source;
uniform lowp vec3 filterCoefficients[16];
uniform lowp mat3 toRGB;

in mediump vec2 coordinates[31];

out lowp vec4 outputColour;

void main(void) {
	vec4 centre = texture(source, coordinates[15]);
	
#define Sample(x) \
	(texture(source, coordinates[x]).rgb - vec3(0.0, 0.5, 0.5)) *	\
	filterCoefficients[x > KernelCentre ? KernelCentre - (x - KernelCentre) : x]

	vec3 channels =
		Sample(0) +		Sample(1) +		Sample(2) +		Sample(3) +
		Sample(4) +		Sample(5) +		Sample(6) +		Sample(7) +
		Sample(8) +		Sample(9) +		Sample(10) +	Sample(11) +
		Sample(12) +	Sample(13) +	Sample(14) +
		filterCoefficients[15] * (centre.rgb - vec3(0.0, 0.5, 0.5)) +
		Sample(16) +	Sample(17) +	Sample(18) +
		Sample(19) +	Sample(20) +	Sample(21) +	Sample(22) +
		Sample(23) +	Sample(24) +	Sample(25) +	Sample(26) +
		Sample(27) +	Sample(28) +	Sample(29) +	Sample(30);

#undef Sample

	outputColour = vec4(
		toRGB * channels,
		1.0
	);
}

)glsl";
}


using namespace Outputs::Display;

namespace {

void enable_vertex_attributes(
	OpenGL::Shader &shader,
	const OpenGL::VertexArray &vertex_array
) {
	OpenGL::DirtyZone zone;
	vertex_array.bind_all();
	const auto enable = [&](const std::string &name, uint16_t &element) {
		shader.enable_vertex_attribute_with_pointer(
			name,
			1,
			GL_UNSIGNED_SHORT,
			GL_FALSE,
			sizeof(zone),
			reinterpret_cast<void *>((reinterpret_cast<uint8_t *>(&element) - reinterpret_cast<uint8_t *>(&zone))),
			1
		);
	};
	enable("zoneBegin", zone.begin);
	enable("zoneEnd", zone.end);
}

template <size_t> struct FilterElement;
template <> struct FilterElement<2> {
	void set_luma(const float luma) { x = luma; }
	void set_chroma(const float chroma) { y = chroma; }
	float x, y;
};
template <> struct FilterElement<3> {
	void set_luma(const float luma) { x = luma; }
	void set_chroma(const float chroma) { y = z = chroma; }
	float x, y, z;
};

template <size_t FilterSize>
void set_common_uniforms(
	OpenGL::Shader &shader,
	const int samples_per_line,
	const int buffer_width,
	const int buffer_height,
	const GLenum source_texture_unit,
	const FilterGenerator::FilterPair filter
) {
	//
	// Set uniforms.
	//
	shader.set_uniform("samplesPerLine", float(samples_per_line));
	shader.set_uniform("bufferSize", float(buffer_width), float(buffer_height));
	shader.set_uniform("source", GLint(source_texture_unit - GL_TEXTURE0));

	// Zip and provide the filter coefficients.
	static_assert(FilterGenerator::MaxKernelSize <= 31);
	FilterElement<FilterSize> elements[31]{};
	filter.luma.copy_to(std::begin(elements), std::end(elements),
		[](const auto iterator, const float coefficient) {
			iterator->set_luma(coefficient);
		}
	);
	filter.chroma.copy_to(std::begin(elements), std::end(elements),
		[](const auto iterator, const float coefficient) {
			iterator->set_chroma(coefficient);
		}
	);

	float packaged_elements[31 * FilterSize];
	static_assert(sizeof(packaged_elements) == sizeof(elements));
	std::memcpy(packaged_elements, elements, sizeof(elements));
	shader.set_uniform("filterCoefficients", FilterSize, 16, packaged_elements);
}

}

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
		separation_fragment_shader,
		dirty_zone_attributes()
	);

	enable_vertex_attributes(shader, vertex_array);
	set_common_uniforms<2>(
		shader,
		samples_per_line,
		buffer_width,
		buffer_height,
		source_texture_unit,
		FilterGenerator(
			samples_per_line,
			per_line_subcarrier_frequency,
			FilterGenerator::DecodingPath::Composite
		).separation_filter()
	);

	return shader;
}

OpenGL::Shader OpenGL::demodulation_shader(
	const OpenGL::API api,
	const ColourSpace colour_space,
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
		demodulation_fragment_shader,
		dirty_zone_attributes()
	);
	enable_vertex_attributes(shader, vertex_array);
	set_common_uniforms<3>(
		shader,
		samples_per_line,
		buffer_width,
		buffer_height,
		source_texture_unit,
		FilterGenerator(
			samples_per_line,
			per_line_subcarrier_frequency,
			FilterGenerator::DecodingPath::Composite	// TODO: pick appropriately.
		).demouldation_filter()
	);
	shader.set_uniform_matrix("toRGB", 3, false, to_rgb_matrix(colour_space).data());

	return shader;
}
