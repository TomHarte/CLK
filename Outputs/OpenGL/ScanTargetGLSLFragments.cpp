//
//  ScanTargetVertexArrayAttributs.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 11/11/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#include "ScanTarget.hpp"

#include <cmath>

#ifndef M_PI
#define M_PI 3.1415926f
#endif

using namespace Outputs::Display::OpenGL;

// MARK: - State setup for compiled shaders.

void ScanTarget::set_uniforms(ShaderType type, Shader &target) const {
	// Slightly over-amping rowHeight here is a cheap way to make sure that lines
	// converge even allowing for the fact that they may not be spaced by exactly
	// the expected distance. Cf. the stencil-powered logic for making sure all
	// pixels are painted only exactly once per field.
	const auto modals = BufferingScanTarget::modals();
	switch(type) {
		case ShaderType::Composition: break;
		default:
			target.set_uniform("rowHeight", GLfloat(1.05f / modals.expected_vertical_lines));
			target.set_uniform("scale", GLfloat(modals.output_scale.x), GLfloat(modals.output_scale.y) * modals.aspect_ratio * (3.0f / 4.0f));
			target.set_uniform("phaseOffset", GLfloat(modals.input_data_tweaks.phase_linked_luminance_offset));

			const float clocks_per_angle = float(modals.cycles_per_line) * float(modals.colour_cycle_denominator) / float(modals.colour_cycle_numerator);
			GLfloat texture_offsets[4];
			GLfloat angles[4];
			for(int c = 0; c < 4; ++c) {
				GLfloat angle = (GLfloat(c) - 1.5f) / 4.0f;
				texture_offsets[c] = angle * clocks_per_angle;
				angles[c] = GLfloat(angle * 2.0f * M_PI);
			}
			target.set_uniform("textureCoordinateOffsets", 1, 4, texture_offsets);
			target.set_uniform("compositeAngleOffsets", 4, 1, angles);

			switch(modals.composite_colour_space) {
				case ColourSpace::YIQ: {
					const GLfloat rgbToYIQ[] = {0.299f, 0.596f, 0.211f, 0.587f, -0.274f, -0.523f, 0.114f, -0.322f, 0.312f};
					const GLfloat yiqToRGB[] = {1.0f, 1.0f, 1.0f, 0.956f, -0.272f, -1.106f, 0.621f, -0.647f, 1.703f};
					target.set_uniform_matrix("lumaChromaToRGB", 3, false, yiqToRGB);
					target.set_uniform_matrix("rgbToLumaChroma", 3, false, rgbToYIQ);
				} break;

				case ColourSpace::YUV: {
					const GLfloat rgbToYUV[] = {0.299f, -0.14713f, 0.615f, 0.587f, -0.28886f, -0.51499f, 0.114f, 0.436f, -0.10001f};
					const GLfloat yuvToRGB[] = {1.0f, 1.0f, 1.0f, 0.0f, -0.39465f, 2.03211f, 1.13983f, -0.58060f, 0.0f};
					target.set_uniform_matrix("lumaChromaToRGB", 3, false, yuvToRGB);
					target.set_uniform_matrix("rgbToLumaChroma", 3, false, rgbToYUV);
				} break;
			}
		break;
	}
}

void ScanTarget::set_sampling_window(int output_width, int, Shader &target) {
	const auto modals = BufferingScanTarget::modals();
	if(modals.display_type != DisplayType::CompositeColour) {
		const float one_pixel_width = float(modals.cycles_per_line) * modals.visible_area.size.width / float(output_width);
		const float clocks_per_angle = float(modals.cycles_per_line) * float(modals.colour_cycle_denominator) / float(modals.colour_cycle_numerator);
		GLfloat texture_offsets[4];
		GLfloat angles[4];
		for(int c = 0; c < 4; ++c) {
			texture_offsets[c] = 1.0f * (((one_pixel_width * float(c)) / 3.0f) - (one_pixel_width * 0.5f));
			angles[c] = GLfloat((texture_offsets[c] / clocks_per_angle) * 2.0f * M_PI);
		}
		target.set_uniform("textureCoordinateOffsets", 1, 4, texture_offsets);
		target.set_uniform("compositeAngleOffsets", 4, 1, angles);
	}
}

void ScanTarget::enable_vertex_attributes(ShaderType type, Shader &target) {
#define rt_offset_of(field, source) (reinterpret_cast<uint8_t *>(&source.field) - reinterpret_cast<uint8_t *>(&source))
	// test_scan and test_line are here so that the byte offsets that need to be
	// calculated inside a loop can be done so validly; offsetof requires constant arguments.
	Scan test_scan;
	Line test_line;

	// Some GPUs require alignment and will need to copy vertex data to a
	// shadow buffer otherwise
	static_assert(sizeof(Scan) % 4 == 0);
	static_assert(sizeof(Line) % 4 == 0);

	switch(type) {
		case ShaderType::Composition:
			for(int c = 0; c < 2; ++c) {
				const std::string prefix = c ? "end" : "start";

				target.enable_vertex_attribute_with_pointer(
					prefix + "DataX",
					1, GL_UNSIGNED_SHORT, GL_FALSE,
					sizeof(Scan),
					reinterpret_cast<void *>(rt_offset_of(scan.end_points[c].data_offset, test_scan)),
					1);

				target.enable_vertex_attribute_with_pointer(
					prefix + "Clock",
					1, GL_UNSIGNED_SHORT, GL_FALSE,
					sizeof(Scan),
					reinterpret_cast<void *>(rt_offset_of(scan.end_points[c].cycles_since_end_of_horizontal_retrace, test_scan)),
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

		default:
			for(int c = 0; c < 2; ++c) {
				const std::string prefix = c ? "end" : "start";

				if(type == ShaderType::Conversion) {
					target.enable_vertex_attribute_with_pointer(
						prefix + "Point",
						2, GL_UNSIGNED_SHORT, GL_FALSE,
						sizeof(Line),
						reinterpret_cast<void *>(rt_offset_of(end_points[c].x, test_line)),
						1);
				}

				target.enable_vertex_attribute_with_pointer(
					prefix + "Clock",
					1, GL_UNSIGNED_SHORT, GL_FALSE,
					sizeof(Line),
					reinterpret_cast<void *>(rt_offset_of(end_points[c].cycles_since_end_of_horizontal_retrace, test_line)),
					1);

				target.enable_vertex_attribute_with_pointer(
					prefix + "CompositeAngle",
					1, GL_SHORT, GL_FALSE,
					sizeof(Line),
					reinterpret_cast<void *>(rt_offset_of(end_points[c].composite_angle, test_line)),
					1);
			}

			target.enable_vertex_attribute_with_pointer(
				"lineY",
				1, GL_UNSIGNED_SHORT, GL_FALSE,
				sizeof(Line),
				reinterpret_cast<void *>(offsetof(Line, line)),
				1);

			target.enable_vertex_attribute_with_pointer(
				"lineCompositeAmplitude",
				1, GL_UNSIGNED_BYTE, GL_FALSE,
				sizeof(Line),
				reinterpret_cast<void *>(offsetof(Line, composite_amplitude)),
				1);
		break;
	}
#undef rt_offset_of
}

std::vector<std::string> ScanTarget::bindings(ShaderType type) const {
	switch(type) {
		case ShaderType::Composition: return {
			"startDataX",
			"startClock",
			"endDataX",
			"endClock",
			"dataY",
			"lineY"
		};

		default: return {
			"startPoint",
			"endPoint",
			"startClock",
			"endClock",
			"lineY",
			"lineCompositeAmplitude",
			"startCompositeAngle",
			"endCompositeAngle"
		};
	}
}

// MARK: - Shader code.

std::string ScanTarget::sampling_function() const {
	std::string fragment_shader;
	const auto modals = BufferingScanTarget::modals();
	const bool is_svideo = modals.display_type == DisplayType::SVideo;

	if(is_svideo) {
		fragment_shader +=
			"vec2 svideo_sample(vec2 coordinate, float angle) {";
	} else {
		fragment_shader +=
			"float composite_sample(vec2 coordinate, float angle) {";
	}

	switch(modals.input_data_type) {
		case InputDataType::Luminance1:
		case InputDataType::Luminance8:
			// Easy, just copy across.
			fragment_shader +=
				is_svideo ?
					"return vec2(textureLod(textureName, coordinate, 0).r, 0.0);" :
					"return textureLod(textureName, coordinate, 0).r;";
		break;

		case InputDataType::PhaseLinkedLuminance8:
			fragment_shader +=
				"uint iPhase = uint(step(sign(angle), 0.0) * 3) ^ uint(abs(angle * 2.0 / 3.141592654) ) & 3u;";

			fragment_shader +=
				is_svideo ?
					"return vec2(textureLod(textureName, coordinate, 0)[iPhase], 0.0);" :
					"return textureLod(textureName, coordinate, 0)[iPhase];";
		break;

		case InputDataType::Luminance8Phase8:
			fragment_shader +=
				"vec2 yc = textureLod(textureName, coordinate, 0).rg;"

				"float phaseOffset = 3.141592654 * 2.0 * 2.0 * yc.y;"
				"float rawChroma = step(yc.y, 0.75) * cos(angle + phaseOffset);";

			fragment_shader +=
				is_svideo ?
					"return vec2(yc.x, rawChroma);" :
					"return mix(yc.x, rawChroma, compositeAmplitude);";
		break;

		case InputDataType::Red1Green1Blue1:
		case InputDataType::Red2Green2Blue2:
		case InputDataType::Red4Green4Blue4:
		case InputDataType::Red8Green8Blue8:
			fragment_shader +=
				"vec3 colour = rgbToLumaChroma * textureLod(textureName, coordinate, 0).rgb;"
				"vec2 quadrature = vec2(cos(angle), sin(angle));";

			fragment_shader +=
				is_svideo ?
					"return vec2(colour.r, dot(quadrature, colour.gb));" :
					"return mix(colour.r, dot(quadrature, colour.gb), compositeAmplitude);";
		break;
	}

	fragment_shader += "}";

	return fragment_shader;
}

std::unique_ptr<Shader> ScanTarget::conversion_shader() const {
	const auto modals = BufferingScanTarget::modals();

	// Compose a vertex shader. If the display type is RGB, generate just the proper
	// geometry position, plus a solitary textureCoordinate.
	//
	// If the display type is anything other than RGB, also produce composite
	// angle and 1/composite amplitude as outputs.
	//
	// If the display type is composite colour, generate four textureCoordinates,
	// spanning a range of -135, -45, +45, +135 degrees.
	//
	// If the display type is S-Video, generate three textureCoordinates, at
	// -45, 0, +45.
	std::string vertex_shader =
		"#version 150\n"

		"uniform vec2 scale;"
		"uniform float rowHeight;"

		"in vec2 startPoint;"
		"in vec2 endPoint;"

		"in float startClock;"
		"in float startCompositeAngle;"
		"in float endClock;"
		"in float endCompositeAngle;"

		"in float lineY;"
		"in float lineCompositeAmplitude;"

		"uniform sampler2D textureName;"
		"uniform sampler2D qamTextureName;"
		"uniform vec2 origin;"
		"uniform vec2 size;"

		"uniform float textureCoordinateOffsets[4];"
		"out vec2 textureCoordinates[4];";

	std::string fragment_shader =
		"#version 150\n"

		"uniform sampler2D textureName;"
		"uniform sampler2D qamTextureName;"

		"in vec2 textureCoordinates[4];"

		"out vec4 fragColour;";

	if(modals.display_type != DisplayType::RGB) {
		vertex_shader +=
			"out float compositeAngle;"
			"out float compositeAmplitude;"
			"out float oneOverCompositeAmplitude;"
		
			"uniform float angleOffsets[4];";
		fragment_shader +=
			"in float compositeAngle;"
			"in float compositeAmplitude;"
			"in float oneOverCompositeAmplitude;"

			"uniform vec4 compositeAngleOffsets;";
	}

	if(modals.display_type == DisplayType::SVideo || modals.display_type == DisplayType::CompositeColour) {
		vertex_shader += "out vec2 qamTextureCoordinates[4];";
		fragment_shader += "in vec2 qamTextureCoordinates[4];";
	}

	// Add the code to generate a proper output position; this applies to all display types.
	vertex_shader +=
		"void main(void) {"
			"float lateral = float(gl_VertexID & 1);"
			"float longitudinal = float((gl_VertexID & 2) >> 1);"
			"vec2 centrePoint = mix(startPoint, vec2(endPoint.x, startPoint.y), lateral) / scale;"
			"vec2 height = normalize(vec2(endPoint.x, startPoint.y) - startPoint).yx * (longitudinal - 0.5) * rowHeight;"
			"vec2 eyePosition = vec2(-1.0, 1.0) + vec2(2.0, -2.0) * (((centrePoint + height) - origin) / size);"
			"gl_Position = vec4(eyePosition, 0.0, 1.0);";

	// For everything other than RGB, calculate the two composite outputs.
	if(modals.display_type != DisplayType::RGB) {
		vertex_shader +=
			"compositeAngle = (mix(startCompositeAngle, endCompositeAngle, lateral) / 32.0) * 3.141592654;"
			"compositeAmplitude = lineCompositeAmplitude / 255.0;"
			"oneOverCompositeAmplitude = mix(0.0, 255.0 / lineCompositeAmplitude, step(0.95, lineCompositeAmplitude));";
	}

	vertex_shader +=
		"float centreClock = mix(startClock, endClock, lateral);"
		"textureCoordinates[0] = vec2(centreClock + textureCoordinateOffsets[0], lineY + 0.5) / textureSize(textureName, 0);"
		"textureCoordinates[1] = vec2(centreClock + textureCoordinateOffsets[1], lineY + 0.5) / textureSize(textureName, 0);"
		"textureCoordinates[2] = vec2(centreClock + textureCoordinateOffsets[2], lineY + 0.5) / textureSize(textureName, 0);"
		"textureCoordinates[3] = vec2(centreClock + textureCoordinateOffsets[3], lineY + 0.5) / textureSize(textureName, 0);";

	if((modals.display_type == DisplayType::SVideo) || (modals.display_type == DisplayType::CompositeColour)) {
		vertex_shader +=
			"float centreCompositeAngle = abs(mix(startCompositeAngle, endCompositeAngle, lateral)) * 4.0 / 64.0;"
			"centreCompositeAngle = floor(centreCompositeAngle);"
			"qamTextureCoordinates[0] = vec2(centreCompositeAngle - 1.5, lineY + 0.5) / textureSize(textureName, 0);"
			"qamTextureCoordinates[1] = vec2(centreCompositeAngle - 0.5, lineY + 0.5) / textureSize(textureName, 0);"
			"qamTextureCoordinates[2] = vec2(centreCompositeAngle + 0.5, lineY + 0.5) / textureSize(textureName, 0);"
			"qamTextureCoordinates[3] = vec2(centreCompositeAngle + 1.5, lineY + 0.5) / textureSize(textureName, 0);";
	}

	vertex_shader += "}";

	// Compose a fragment shader.

	if(modals.display_type != DisplayType::RGB) {
		fragment_shader +=
			"uniform mat3 lumaChromaToRGB;"
			"uniform mat3 rgbToLumaChroma;";

		fragment_shader += sampling_function();
	}

	fragment_shader +=
		"void main(void) {"
			"vec3 fragColour3;";

	switch(modals.display_type) {
		case DisplayType::CompositeColour:
			fragment_shader += R"x(
				vec4 angles = compositeAngle + compositeAngleOffsets;

				// Sample four times over, at proper angle offsets.
				vec4 samples = vec4(
					composite_sample(textureCoordinates[0], angles.x),
					composite_sample(textureCoordinates[1], angles.y),
					composite_sample(textureCoordinates[2], angles.z),
					composite_sample(textureCoordinates[3], angles.w)
				);

				// The outer structure of the OpenGL scan target means in practice that
				// compositeAmplitude will be the same value across a piece of
				// geometry. I am therefore optimistic that this conditional will not
				// cause a divergence in fragment execution.
				if(compositeAmplitude < 0.01) {
					// Compute only a luminance for use if there's no colour information.
					fragColour3 = vec3(dot(samples, vec4(0.15, 0.35, 0.35, 0.15)));
				} else {
					// Take the average to calculate luminance, then subtract that from all four samples to
					// give chrominance.
					float luminance = dot(samples, vec4(0.25));

					// Split and average chrominance.
					vec2 chrominances[4] = vec2[4](
						textureLod(qamTextureName, qamTextureCoordinates[0], 0).gb,
						textureLod(qamTextureName, qamTextureCoordinates[1], 0).gb,
						textureLod(qamTextureName, qamTextureCoordinates[2], 0).gb,
						textureLod(qamTextureName, qamTextureCoordinates[3], 0).gb
					);
					vec2 channels = (chrominances[0] + chrominances[1] + chrominances[2] + chrominances[3])*0.5 - vec2(1.0);

					// Apply a colour space conversion to get RGB.
					fragColour3 = lumaChromaToRGB * vec3(luminance / (1.0 - compositeAmplitude), channels);
				}
			)x";
		break;

		case DisplayType::CompositeMonochrome:
			fragment_shader +=
				"vec4 angles = compositeAngle + compositeAngleOffsets;"
				"vec4 samples = vec4("
					"composite_sample(textureCoordinates[0], angles.x),"
					"composite_sample(textureCoordinates[1], angles.y),"
					"composite_sample(textureCoordinates[2], angles.z),"
					"composite_sample(textureCoordinates[3], angles.w)"
				");"
				"fragColour3 = vec3(dot(samples, vec4(0.15, 0.35, 0.35, 0.25)));";
		break;

		case DisplayType::RGB:
			fragment_shader +=
				"vec3 samples[4] = vec3[4]("
					"textureLod(textureName, textureCoordinates[0], 0).rgb,"
					"textureLod(textureName, textureCoordinates[1], 0).rgb,"
					"textureLod(textureName, textureCoordinates[2], 0).rgb,"
					"textureLod(textureName, textureCoordinates[3], 0).rgb"
				");"
				"fragColour3 = samples[0]*0.15 + samples[1]*0.35 + samples[2]*0.35 + samples[2]*0.15;";
		break;

		case DisplayType::SVideo:
			fragment_shader +=
				// Sample the S-Video stream to obtain luminance.
				"vec4 angles = compositeAngle + compositeAngleOffsets;"
				"vec4 samples = vec4("
					"svideo_sample(textureCoordinates[0], angles.x).x,"
					"svideo_sample(textureCoordinates[1], angles.y).x,"
					"svideo_sample(textureCoordinates[2], angles.z).x,"
					"svideo_sample(textureCoordinates[3], angles.w).x"
				");"
				"float luminance = dot(samples, vec4(0.15, 0.35, 0.35, 0.25));"

				// Split and average chrominaxnce.
				"vec2 chrominances[4] = vec2[4]("
					"textureLod(qamTextureName, qamTextureCoordinates[0], 0).gb,"
					"textureLod(qamTextureName, qamTextureCoordinates[1], 0).gb,"
					"textureLod(qamTextureName, qamTextureCoordinates[2], 0).gb,"
					"textureLod(qamTextureName, qamTextureCoordinates[3], 0).gb"
				");"
				"vec2 channels = (chrominances[0] + chrominances[1] + chrominances[2] + chrominances[3])*0.5 - vec2(1.0);"

				// Apply a colour space conversion to get RGB.
				"fragColour3 = lumaChromaToRGB * vec3(luminance, channels);";
		break;
	}

	// Apply a brightness adjustment if requested.
	if(fabs(modals.brightness - 1.0f) > 0.05f) {
		fragment_shader += "fragColour3 = fragColour3 * " + std::to_string(modals.brightness) + ";";
	}

	// Apply a gamma correction if required.
	if(fabs(output_gamma_ - modals.intended_gamma) > 0.05f) {
		const float gamma_ratio = output_gamma_ / modals.intended_gamma;
		fragment_shader += "fragColour3 = pow(fragColour3, vec3(" + std::to_string(gamma_ratio) + "));";
	}

	fragment_shader +=
			"fragColour = vec4(fragColour3, 0.64);"
		"}";

	return std::make_unique<Shader>(
		vertex_shader,
		fragment_shader,
		bindings(ShaderType::Conversion)
	);
}

std::unique_ptr<Shader> ScanTarget::composition_shader() const {
	const auto modals = BufferingScanTarget::modals();
	const std::string vertex_shader =
	R"x(#version 150

		in float startDataX;
		in float startClock;

		in float endDataX;
		in float endClock;

		in float dataY;
		in float lineY;

		out vec2 textureCoordinate;
		uniform usampler2D textureName;

		void main(void) {
			float lateral = float(gl_VertexID & 1);
			float longitudinal = float((gl_VertexID & 2) >> 1);

			textureCoordinate = vec2(mix(startDataX, endDataX, lateral), dataY + 0.5) / textureSize(textureName, 0);
			vec2 eyePosition = vec2(mix(startClock, endClock, lateral), lineY + longitudinal) / vec2(2048.0, 2048.0);
			gl_Position = vec4(eyePosition*2.0 - vec2(1.0), 0.0, 1.0);
		}
	)x";

	std::string fragment_shader =
	R"x(#version 150

		out vec4 fragColour;
		in vec2 textureCoordinate;

		uniform usampler2D textureName;

		void main(void) {
	)x";

	switch(modals.input_data_type) {
		case InputDataType::Luminance1:
			fragment_shader += "fragColour = textureLod(textureName, textureCoordinate, 0).rrrr;";
		break;

		case InputDataType::Luminance8:
			fragment_shader += "fragColour = textureLod(textureName, textureCoordinate, 0).rrrr / vec4(255.0);";
		break;

		case InputDataType::PhaseLinkedLuminance8:
		case InputDataType::Luminance8Phase8:
		case InputDataType::Red8Green8Blue8:
			fragment_shader += "fragColour = textureLod(textureName, textureCoordinate, 0) / vec4(255.0);";
		break;

		case InputDataType::Red1Green1Blue1:
			fragment_shader += "fragColour = vec4(textureLod(textureName, textureCoordinate, 0).rrr & uvec3(4u, 2u, 1u), 1.0);";
		break;

		case InputDataType::Red2Green2Blue2:
			fragment_shader +=
				"uint textureValue = textureLod(textureName, textureCoordinate, 0).r;"
				"fragColour = vec4(float((textureValue >> 4) & 3u), float((textureValue >> 2) & 3u), float(textureValue & 3u), 3.0) / 3.0;";
		break;

		case InputDataType::Red4Green4Blue4:
			fragment_shader +=
				"uvec2 textureValue = textureLod(textureName, textureCoordinate, 0).rg;"
				"fragColour = vec4(float(textureValue.r) / 15.0, float(textureValue.g & 240u) / 240.0, float(textureValue.g & 15u) / 15.0, 1.0);";
		break;
	}

	return std::make_unique<Shader>(
		vertex_shader,
		fragment_shader + "}",
		bindings(ShaderType::Composition)
	);
}

std::unique_ptr<Shader> ScanTarget::qam_separation_shader() const {
	const auto modals = BufferingScanTarget::modals();
	const bool is_svideo = modals.display_type == DisplayType::SVideo;

	// Sets up texture coordinates to run between startClock and endClock, mapping to
	// coordinates that correlate with four times the absolute value of the composite angle.
	std::string vertex_shader =
		"#version 150\n"

		"in float startClock;"
		"in float startCompositeAngle;"
		"in float endClock;"
		"in float endCompositeAngle;"

		"in float lineY;"
		"in float lineCompositeAmplitude;"

		"uniform sampler2D textureName;"
		"uniform float textureCoordinateOffsets[4];"

		"out float compositeAngle;"
		"out float compositeAmplitude;"
		"out float oneOverCompositeAmplitude;";

	std::string fragment_shader =
		"#version 150\n"

		"uniform sampler2D textureName;"
		"uniform mat3 rgbToLumaChroma;"

		"in float compositeAngle;"
		"in float compositeAmplitude;"
		"in float oneOverCompositeAmplitude;"

		"out vec4 fragColour;"
		"uniform vec4 compositeAngleOffsets;";

	if(is_svideo) {
		vertex_shader += "out vec2 textureCoordinate;";
		fragment_shader += "in vec2 textureCoordinate;";
	} else {
		vertex_shader += "out vec2 textureCoordinates[4];";
		fragment_shader += "in vec2 textureCoordinates[4];";
	}

	vertex_shader +=
		"void main(void) {"
			"float lateral = float(gl_VertexID & 1);"
			"float longitudinal = float((gl_VertexID & 2) >> 1);"
			"float centreClock = mix(startClock, endClock, lateral);"

			"compositeAngle = mix(startCompositeAngle, endCompositeAngle, lateral) / 64.0;"

			"float snappedCompositeAngle = floor(abs(compositeAngle) * 4.0);"
			"vec2 eyePosition = vec2(snappedCompositeAngle, lineY + longitudinal) / vec2(2048.0, 2048.0);"
			"gl_Position = vec4(eyePosition*2.0 - vec2(1.0), 0.0, 1.0);"

			"compositeAngle = compositeAngle * 2.0 * 3.141592654;"
			"compositeAmplitude = lineCompositeAmplitude / 255.0;"
			"oneOverCompositeAmplitude = mix(0.0, 255.0 / lineCompositeAmplitude, step(0.95, lineCompositeAmplitude));";

	if(is_svideo) {
		vertex_shader +=
			"textureCoordinate = vec2(centreClock, lineY + 0.5) / textureSize(textureName, 0);";
	} else {
		vertex_shader +=
			"textureCoordinates[0] = vec2(centreClock + textureCoordinateOffsets[0], lineY + 0.5) / textureSize(textureName, 0);"
			"textureCoordinates[1] = vec2(centreClock + textureCoordinateOffsets[1], lineY + 0.5) / textureSize(textureName, 0);"
			"textureCoordinates[2] = vec2(centreClock + textureCoordinateOffsets[2], lineY + 0.5) / textureSize(textureName, 0);"
			"textureCoordinates[3] = vec2(centreClock + textureCoordinateOffsets[3], lineY + 0.5) / textureSize(textureName, 0);";
	}

	vertex_shader += "}";

	fragment_shader +=
		sampling_function() +
		"void main(void) {";

	if(modals.display_type == DisplayType::SVideo) {
		fragment_shader +=
			"fragColour = vec4(svideo_sample(textureCoordinate, compositeAngle).rgg * vec3(1.0, cos(compositeAngle), sin(compositeAngle)), 1.0);";
	} else {
			fragment_shader +=
				"vec4 angles = compositeAngle + compositeAngleOffsets;"

				// Sample four times over, at proper angle offsets.
				"vec4 samples = vec4("
					"composite_sample(textureCoordinates[0], angles.x),"
					"composite_sample(textureCoordinates[1], angles.y),"
					"composite_sample(textureCoordinates[2], angles.z),"
					"composite_sample(textureCoordinates[3], angles.w)"
				");"

				// Take the average to calculate luminance, then subtract that from all four samples to
				// give chrominance.
				"float luminance = dot(samples, vec4(0.25));"
				"float chrominance = (dot(samples.yz, vec2(0.5)) - luminance) * oneOverCompositeAmplitude;"

				// Pack that all up and send it on its way.
				"fragColour = vec4(luminance, vec2(cos(compositeAngle), sin(compositeAngle)) * chrominance, 1.0);";
	};

	fragment_shader +=
			"fragColour = fragColour*0.5 + vec4(0.5);"
		"}";

	return std::make_unique<Shader>(
		vertex_shader,
		fragment_shader,
		bindings(ShaderType::QAMSeparation)
	);
}
