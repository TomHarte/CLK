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

// TODO: limit below to 8 vec4 varyings; per my interpretation of the alignment rules I think that allows
// 16 vec2 varyings. Spacing out the coordinates to provide suitable caching hints should avoid a significant penalty
// for the other 'dependent' reads.
constexpr char vertex_shader[] = R"glsl(

uniform float samplesPerLine;
uniform vec2 bufferSize;

in float zoneBegin;
in float zoneEnd;

#ifdef USES_COORDINATES
out vec2 coordinates[11];
#endif

void main(void) {
	float lateral = float(gl_VertexID & 1);
	float longitudinal = float((gl_VertexID & 2) >> 1);

	float sampleY = bufferSize.y - mix(zoneBegin, zoneEnd, longitudinal);
	float centreX = lateral * samplesPerLine;

	// Factors here:
	//
	//	(1)	only 8 vec4 varyings are guaranteed to exist, which can be utilised as 16 vec2s.
	//		So there aren't enough to guarantee one varying per sample location;
	//	(2)	the cost of dependent reads is negligible nowadays unless and until it obviates
	//		the cache.
	//
	// So the coordinates picked are a spread across the area being sampled to provide enough
	// information that the GPU should be able to cache efficiently.
#ifdef USES_COORDINATES
	coordinates[0] = vec2(centreX - 14.0, sampleY) / bufferSize;		// for 15, 14, 13		[0, 1, 2]
	coordinates[1] = vec2(centreX - 11.0, sampleY) / bufferSize;		// for 12, 11, 10		[3, 4, 5]
	coordinates[2] = vec2(centreX - 8.0, sampleY) / bufferSize;			// for 9, 8, 7			[6, 7, 8]
	coordinates[3] = vec2(centreX - 5.0, sampleY) / bufferSize;			// for 6, 5, 4			[9, 10, 11]
	coordinates[4] = vec2(centreX - 2.0, sampleY) / bufferSize;			// for 3, 2, 1			[12, 13, 14]
	coordinates[5] = vec2(centreX + 0.0, sampleY) / bufferSize;	// Centre.						[15]
	coordinates[6] = vec2(centreX + 2.0, sampleY) / bufferSize;			// 1, 2, 3				[16, 17, 18]
	coordinates[7] = vec2(centreX + 5.0, sampleY) / bufferSize;			// 4, 5, 6				[19, 20, 21]
	coordinates[8] = vec2(centreX + 8.0, sampleY) / bufferSize;			// 7, 8, 9				[22, 23, 24]
	coordinates[9] = vec2(centreX + 11.0, sampleY) / bufferSize;		// 10, 11, 12			[25, 26, 27]
	coordinates[10] = vec2(centreX + 14.0, sampleY) / bufferSize;		// 13, 14, 15			[28, 29, 30]
#endif

	gl_Position = vec4(
		(vec2(centreX, sampleY) / bufferSize - vec2(0.5)) * vec2(2.0),
		0.0,
		1.0
	);
}

)glsl";

constexpr char coordinate_indexer[] = R"glsl(
#define KernelCentre 15

in vec2 coordinates[11];
uniform vec2 bufferSize;

#define offset(i) ((float(i) - 15.0) / bufferSize.x)

#define coordinate(i) (\
	(i) == 1 ? coordinates[0] : \
	(i) == 4 ? coordinates[1] : \
	(i) == 7 ? coordinates[2] : \
	(i) == 10 ? coordinates[3] : \
	(i) == 13 ? coordinates[4] : \
	(i) == 15 ? coordinates[5] : \
	(i) == 17 ? coordinates[6] : \
	(i) == 20 ? coordinates[7] : \
	(i) == 23 ? coordinates[8] : \
	(i) == 26 ? coordinates[9] : \
	(i) == 29 ? coordinates[10] : \
	coordinates[5] + vec2(offset(i), 0.0) \
)

#define coefficient(x) filterCoefficients[x > KernelCentre ? KernelCentre - (x - KernelCentre) : x]

)glsl";


constexpr char separation_fragment_shader[] = R"glsl(

uniform sampler2D source;
uniform vec2 filterCoefficients[16];

out vec4 outputColour;

void main(void) {
	vec4 centre = texture(source, coordinate(15));

#define Sample(x) texture(source, coordinate(x)).r * coefficient(x)

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

	float colourAmplitude = centre.a;
	float isColour = step(0.01, colourAmplitude);
	float chromaScale = mix(1.0, colourAmplitude, isColour);
	float lumaScale = mix(1.0, 1.0 - colourAmplitude * 2.0, isColour);
	outputColour = vec4(
		(channels.x - colourAmplitude) / lumaScale,
		isColour * channels.y * (centre.yz / chromaScale) + vec2(0.5),
		1.0
	);
}

)glsl";

constexpr char demodulation_fragment_shader[] = R"glsl(
uniform sampler2D source;
uniform vec3 filterCoefficients[16];
uniform mat3 toRGB;

out vec4 outputColour;

void main(void) {
	vec4 centre = texture(source, coordinate(15));
	
#define Sample(x) (texture(source, coordinate(x)).rgb - vec3(0.0, 0.5, 0.5)) * coefficient(x)

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

constexpr char fill_fragment_shader[] = R"glsl(
uniform vec4 colour;
out vec4 outputColour;

void main(void) {
	outputColour = colour;
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

void set_size_uniforms(
	OpenGL::Shader &shader,
	const int samples_per_line,
	const int buffer_width,
	const int buffer_height
) {
	shader.set_uniform("samplesPerLine", float(samples_per_line));
	shader.set_uniform("bufferSize", float(buffer_width), float(buffer_height));
}

template <size_t FilterSize>
void set_filter_uniforms(
	OpenGL::Shader &shader,
	const int samples_per_line,
	const int buffer_width,
	const int buffer_height,
	const GLenum source_texture_unit,
	const FilterGenerator::FilterPair filter
) {
	set_size_uniforms(shader, samples_per_line, buffer_width, buffer_height);
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
		std::string("#define USES_COORDINATES\n") + vertex_shader,
		std::string(coordinate_indexer) + separation_fragment_shader,
		dirty_zone_attributes()
	);

	enable_vertex_attributes(shader, vertex_array);
	set_filter_uniforms<2>(
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
	const DisplayType display_type,
	const float per_line_subcarrier_frequency,
	const int samples_per_line,
	const int buffer_width,
	const int buffer_height,
	const VertexArray &vertex_array,
	const GLenum source_texture_unit
) {
	auto shader = OpenGL::Shader(
		api,
		std::string("#define USES_COORDINATES\n") + vertex_shader,
		std::string(coordinate_indexer) + demodulation_fragment_shader,
		dirty_zone_attributes()
	);
	enable_vertex_attributes(shader, vertex_array);
	set_filter_uniforms<3>(
		shader,
		samples_per_line,
		buffer_width,
		buffer_height,
		source_texture_unit,
		FilterGenerator(
			samples_per_line,
			per_line_subcarrier_frequency,
			is_composite(display_type) ?
				FilterGenerator::DecodingPath::Composite :
				FilterGenerator::DecodingPath::SVideo
		).demouldation_filter()
	);
	shader.set_uniform_matrix("toRGB", 3, false, to_rgb_matrix(colour_space).data());

	return shader;
}

OpenGL::FillShader::FillShader(
	const API api,
	const int samples_per_line,
	const int buffer_width,
	const int buffer_height,
	const VertexArray &vertex_array
) {
	shader_ = OpenGL::Shader(
		api,
		vertex_shader,
		fill_fragment_shader,
		dirty_zone_attributes()
	);
	enable_vertex_attributes(shader_, vertex_array);
	set_size_uniforms(shader_, samples_per_line, buffer_width, buffer_height);
}

void OpenGL::FillShader::bind(const float r, const float g, const float b, const float a) {
	shader_.bind();
	if(colour_[0] != r || colour_[1] != g || colour_[2] != b || colour_[3] != a) {
		colour_[0] = r;
		colour_[1] = g;
		colour_[2] = b;
		colour_[3] = a;
		shader_.set_uniform("colour", r, g, b, a);
	}
}
