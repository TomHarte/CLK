//
//  CompositionShader.cpp
//  Clock Signal Kiosk
//
//  Created by Thomas Harte on 26/01/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include "CompositionShader.hpp"

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

constexpr char vertex_shader[] = R"glsl(

uniform sampler2D source;

in mediump float startDataX;
in float startClock;

in float endDataX;
in float endClock;

in float dataY;
in float lineY;

out mediump vec2 coordinate;
out highp float phase;
out lowp float compositeAmplitude;

void main(void) {
	float lateral = float(gl_VertexID & 1);
	float longitudinal = float((gl_VertexID & 2) >> 1);

	coordinate = vec2(mix(startDataX, endDataX, lateral), dataY + 0.5) / vec2(textureSize(source, 0));
	phase = 0;
	compositeAmplitude = 0.16;

	vec2 eyePosition = vec2(mix(startClock, endClock, lateral), lineY + longitudinal) / vec2(2048.0, 2048.0);
	gl_Position = vec4(eyePosition*2.0 - vec2(1.0), 0.0, 1.0);
}

)glsl";

constexpr char fragment_shader[] = R"glsl(

uniform lowp mat3 fromRGB;

in mediump vec2 coordinate;
in highp float phase;
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
		int offset = int(floor(phase * 4.0));
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
		int offset = int(floor(coordinate * 4.0));
		return vec4(
			source[offset],
			quadrature(),
			compositeAmplitude
		);
	}

#endif



#ifdef SYNTHESISE_COMPOSITE

	#ifdef SYNTHESISE_SVIDEO

		lowp vec4 sample_composite() {
			lowp vec3 colour = fromRGB * sample_rgb();
			lowp vec2 q = quadrature();

			lowp float chroma = q * colour.gb;

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
		lowp float chroma = q * colour.gb;

		return vec4(
			colour.r,
			chroma,
			q
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

using namespace Outputs::Display::OpenGL;

CompositionShader::CompositionShader() {
	const auto prefix =
		std::string() +
		"#define INPUT_LUMINANCE8_PHASE8\n" +
		"#define OUTPUT_SVIDEO\n";

	const auto vertex = prefix + vertex_shader;
	const auto fragment = prefix + fragment_shader;

	Shader test(
		API::OpenGL32Core,
		vertex,
		fragment
	);
	(void)test;
}

