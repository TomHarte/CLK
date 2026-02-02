//
//  CompositionShader.cpp
//  Clock Signal Kiosk
//
//  Created by Thomas Harte on 26/01/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include "CompositionShader.hpp"
#include "Outputs/ScanTargets/BufferingScanTarget.hpp"

namespace {

// To compile the below shader programs:
//
//	(1)	#define output type; one of:
//			OUTPUT_COMPOSITE
//			OUTPUT_SVIDEO
//			OUTPUT_RGB
//	(2)	#define the input format; one of:
//			INPUT_LUMINANCE1
//			INPUT_LUMINANCE8
//			INPUT_PHASE_LINKED_LUMINANCE8
//			INPUT_LUMINANCE8_PHASE8
//			INPUT_RED1_GREEN1_BLUE1
//			INPUT_RED2_GREEN2_BLUE2
//			INPUT_RED4_GREEN4_BLUE4
//			INPUT_RED8_GREEN8_BLUE8
//	(3)	#define NO_BITWISE to perform sampling with floating
//		point operations only. Those versions are slower in principle,
//		but obviously faster if the target hardware is using
//		ES 2 or original WebGL and therefore isn't guaranteed to
//		support integers or bitwise operations.
//

// TODO: implement NO_BITWISE.

constexpr char vertex_shader[] = R"glsl(

uniform mediump float cyclesSinceRetraceMultiplier;
uniform mediump vec2 sourceSize;
uniform mediump vec2 targetSize;

in mediump float scanEndpoint0CyclesSinceRetrace;
in mediump float scanEndpoint0DataOffset;
in mediump float scanEndpoint0CompositeAngle;

in mediump float scanEndpoint1CyclesSinceRetrace;
in mediump float scanEndpoint1DataOffset;
in mediump float scanEndpoint1CompositeAngle;

in mediump float scanDataY;
in mediump float scanLine;
in mediump float scanCompositeAmplitude;

out mediump vec2 coordinate;
out highp float phase;
out highp float unitPhase;
out lowp float compositeAmplitude;

void main(void) {
	float lateral = float(gl_VertexID & 1);
	float longitudinal = float((gl_VertexID & 2) >> 1);

	// Texture: interpolates x = [start -> end]DataX; y = dataY.
	coordinate = vec2(
		mix(
			scanEndpoint0DataOffset,
			scanEndpoint1DataOffset,
			lateral
		),
		scanDataY + 0.5
	) / sourceSize;

	// Phase and amplitude.
	unitPhase = mix(
		scanEndpoint0CompositeAngle,
		scanEndpoint1CompositeAngle,
		lateral
	) / 64.0;
	phase = 2.0 * 3.141592654 * unitPhase;
	compositeAmplitude = scanCompositeAmplitude;

	// Position: inteprolates x = [start -> end]Clock; y = line.
	vec2 eyePosition = vec2(
		mix(
			scanEndpoint0CyclesSinceRetrace,
			scanEndpoint1CyclesSinceRetrace,
			lateral
		) * cyclesSinceRetraceMultiplier,
		scanLine + longitudinal
	) / targetSize;
	eyePosition = vec2(
		lateral * 0.5,
		(scanLine + longitudinal * 512.0) / targetSize.y
	);
	gl_Position = vec4(
		eyePosition * vec2(2.0, -2.0) + vec2(-1.0, 1.0),
		0.0,
		1.0
	);
}

)glsl";

constexpr char fragment_shader[] = R"glsl(

uniform lowp mat3 fromRGB;

in mediump vec2 coordinate;
in highp float phase;
in highp float unitPhase;
in lowp float compositeAmplitude;

lowp vec2 quadrature() {
	return vec2(cos(phase), sin(phase));
}



#ifdef INPUT_LUMINANCE1

	uniform sampler2D source;

	lowp vec4 sample_composite() {
		return vec4(
			clamp(texture(source, coordinate).r * 255.0, 0.0, 1.0),
			quadrature(),
			compositeAmplitude
		);
	}

#endif



#ifdef INPUT_LUMINANCE8

	uniform sampler2D source;

	lowp vec4 sample_composite() {
		return vec4(
			texture(source, coordinate).r,
			quadrature(),
			compositeAmplitude
		);
	}

#endif



#ifdef INPUT_PHASE_LINKED_LUMINANCE8

	uniform sampler2D source;

	lowp vec4 sample_composite() {
		vec4 source = texture(source, coordinate);
		int offset = int(floor(unitPhase * 4.0)) & 3;
		return vec4(
			source[offset],
			quadrature(),
			compositeAmplitude
		);
	}

#endif



#ifdef INPUT_LUMINANCE8_PHASE8

	uniform sampler2D source;
	#define SYNTHESISE_COMPOSITE

	lowp vec4 sample_svideo() {
		lowp vec2 source = texture(source, coordinate).rg;
		float chroma = cos(phase + source.g); 
		lowp vec2 q = quadrature();
		
		return vec4(
			source.r,
			chroma * q,
			1.0
		);
	}

#endif



#ifdef INPUT_RED1_GREEN1_BLUE1

	uniform usampler2D source;
	#define SYNTHESISE_SVIDEO
	#define SYNTHESISE_COMPOSITE

	lowp vec3 sample_rgb() {
		uvec3 colour = texture(source, coordinate).rrr & uvec3(4u, 2u, 1u);
		return clamp(vec3(colour), 0.0, 1.0);
	}

#endif



#ifdef INPUT_RED2_GREEN2_BLUE2

	uniform usampler2D source;
	#define SYNTHESISE_SVIDEO
	#define SYNTHESISE_COMPOSITE

	lowp vec3 sample_rgb() {
		uint colour = texture(source, coordinate).r;
		return vec3(
			float((colour >> 4) & 3u),
			float((colour >> 2) & 3u),
			float((colour >> 0) & 3u)
		) / 3.0;
	}

#endif



#ifdef INPUT_RED4_GREEN4_BLUE4

	uniform usampler2D source;
	#define SYNTHESISE_SVIDEO
	#define SYNTHESISE_COMPOSITE

	lowp vec3 sample_rgb() {
		uvec2 colour = texture(source, coordinate).rg;
		return vec3(
			float(colour.r) / 15.0,
			float(colour.g & 240u) / 240.0,
			float(colour.g & 15u) / 15.0
		);
	}

#endif



#ifdef INPUT_RED8_GREEN8_BLUE8

	uniform sampler2D source;
	#define SYNTHESISE_SVIDEO
	#define SYNTHESISE_COMPOSITE

	lowp vec3 sample_rgb() {
		return texture(source, coordinate).rgb;
	}

#endif



#ifdef SYNTHESISE_COMPOSITE

	#ifdef SYNTHESISE_SVIDEO

		lowp vec4 sample_composite() {
			lowp vec3 colour = fromRGB * sample_rgb();
			lowp vec2 q = quadrature();

			lowp float chroma = dot(q, colour.gb);

			return vec4(
				colour.r * (1.0 - 2.0 * compositeAmplitude)  + chroma * compositeAmplitude,
				q,
				compositeAmplitude
			);
		}

	#else

		lowp vec4 sample_composite() {
			lowp vec4 colour = sample_svideo();

			return vec4(
				colour.r * (1.0 - 2.0 * compositeAmplitude)  + colour.g * compositeAmplitude,
				colour.ba,
				compositeAmplitude
			);
		}

	#endif

#endif



#ifdef SYNTHESISE_SVIDEO

	lowp vec4 sample_svideo() {
		lowp vec3 colour = fromRGB * sample_rgb();
		lowp vec2 q = quadrature();
		lowp float chroma = dot(q, colour.gb);

		return vec4(
			colour.r,
			chroma * q,
			1.0
		);
	}

#endif



out lowp vec4 outputColour;

void main(void) {

#ifdef OUTPUT_COMPOSITE
	outputColour = sample_composite();
#endif

#ifdef OUTPUT_SVIDEO
	outputColour = sample_svideo();
#endif

#ifdef OUTPUT_RGB
	outputColour = vec4(sample_rgb(), 1.0);
#endif

}

)glsl";
}

using namespace Outputs::Display;

OpenGL::Shader OpenGL::composition_shader(
	const OpenGL::API api,
	const InputDataType input,
	const DisplayType display,
	const ColourSpace colour_space,
	const float cycles_multiplier,
	const int source_width,
	const int source_height,
	const int target_width,
	const int target_height,
	const VertexArray &vertex_array,
	const GLenum source_texture_unit
) {
	//
	// Compose and compiler shader.
	//
	std::string prefix;

	prefix += "#define INPUT_";
	prefix += [&] {
		switch(input) {
			case InputDataType::Luminance1: return "LUMINANCE1";
			case InputDataType::Luminance8: return "LUMINANCE8";
			case InputDataType::PhaseLinkedLuminance8: return "PHASE_LINKED_LUMINANCE8";
			case InputDataType::Luminance8Phase8: return "LUMINANCE8_PHASE8";
			case InputDataType::Red1Green1Blue1: return "RED1_GREEN1_BLUE1";
			case InputDataType::Red2Green2Blue2: return "RED2_GREEN2_BLUE2";
			case InputDataType::Red4Green4Blue4: return "RED4_GREEN4_BLUE4";
			case InputDataType::Red8Green8Blue8: return "RED8_GREEN8_BLUE8";
		}
		__builtin_unreachable();
	} ();
	prefix += "\n";

	prefix += "#define OUTPUT_";
	prefix += [&] {
		switch(display) {
			case DisplayType::RGB: return "RGB";
			case DisplayType::SVideo: return "SVIDEO";
			case DisplayType::CompositeColour:
			case DisplayType::CompositeMonochrome:
				return "COMPOSITE";
		}
		__builtin_unreachable();
	} ();
	prefix += "\n";

	if(!supports_bitwise_operations(api)) {
		prefix += "#define NO_BITWISE\n";
	}

	auto shader = OpenGL::Shader(
		api,
		prefix + vertex_shader,
		prefix + fragment_shader
	);

	//
	// Enable vertex attributes.
	//
	BufferingScanTarget::Scan scan;
	vertex_array.bind();
	const auto enable = [&](const std::string &name, auto &element, const bool normalise) {
		assert(sizeof(element) == 1 || sizeof(element) == 2);
		shader.enable_vertex_attribute_with_pointer(
			name,
			1,
			sizeof(element) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_BYTE,
			normalise ? GL_TRUE : GL_FALSE,
			sizeof(scan),
			reinterpret_cast<void *>((reinterpret_cast<uint8_t *>(&element) - reinterpret_cast<uint8_t *>(&scan))),
			1
		);
	};

	for(int c = 0; c < 2; c++) {
		const std::string endpoint = std::string("scanEndpoint") + std::to_string(c);
		enable(endpoint + "DataOffset", scan.scan.end_points[c].data_offset, false);
		enable(endpoint + "CyclesSinceRetrace", scan.scan.end_points[c].cycles_since_end_of_horizontal_retrace, false);
		enable(endpoint + "CompositeAngle", scan.scan.end_points[c].composite_angle, false);
	}

	enable("scanDataY", scan.data_y, false);
	enable("scanLine", scan.line, false);
	enable("scanCompositeAmplitude", scan.scan.composite_amplitude, true);

	//
	// Set uniforms.
	//
	shader.set_uniform("cyclesSinceRetraceMultiplier", cycles_multiplier);
	shader.set_uniform("sourceSize", float(source_width), float(source_height));
	shader.set_uniform("targetSize", float(target_width), float(target_height));
	shader.set_uniform("source", GLint(source_texture_unit - GL_TEXTURE0));
	shader.set_uniform_matrix("fromRGB", 3, false, from_rgb_matrix(colour_space).data());

	return shader;
}
