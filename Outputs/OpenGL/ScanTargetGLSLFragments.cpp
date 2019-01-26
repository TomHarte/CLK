//
//  ScanTargetVertexArrayAttributs.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 11/11/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#include "ScanTarget.hpp"

#include <cmath>

using namespace Outputs::Display::OpenGL;

void Outputs::Display::OpenGL::ScanTarget::set_uniforms(ShaderType type, Shader &target) {
	// Slightly over-amping rowHeight here is a cheap way to make sure that lines
	// converge even allowing for the fact that they may not be spaced by exactly
	// the expected distance. Cf. the stencil-powered logic for making sure all
	// pixels are painted only exactly once per field.
	target.set_uniform("rowHeight", GLfloat(1.05f / modals_.expected_vertical_lines));
	target.set_uniform("scale", GLfloat(modals_.output_scale.x), GLfloat(modals_.output_scale.y));
	target.set_uniform("phaseOffset", GLfloat(modals_.input_data_tweaks.phase_linked_luminance_offset));
}

void ScanTarget::enable_vertex_attributes(ShaderType type, Shader &target) {
#define rt_offset_of(field, source) (reinterpret_cast<uint8_t *>(&source.field) - reinterpret_cast<uint8_t *>(&source))
	// test_scan and test_line are here so that the byte offsets that need to be
	// calculated inside a loop can be done so validly; offsetof requires constant arguments.
	Scan test_scan;
	Line test_line;

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

		case ShaderType::Conversion:
			for(int c = 0; c < 2; ++c) {
				const std::string prefix = c ? "end" : "start";

				target.enable_vertex_attribute_with_pointer(
					prefix + "Point",
					2, GL_UNSIGNED_SHORT, GL_FALSE,
					sizeof(Line),
					reinterpret_cast<void *>(rt_offset_of(end_points[c].x, test_line)),
					1);

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

std::unique_ptr<Shader> ScanTarget::composition_shader() const {
	const std::string vertex_shader =
		"#version 150\n"

		"in float startDataX;"
		"in float startClock;"

		"in float endDataX;"
		"in float endClock;"

		"in float dataY;"
		"in float lineY;"

		"out vec2 textureCoordinate;"
		"uniform usampler2D textureName;"

		"void main(void) {"
			"float lateral = float(gl_VertexID & 1);"
			"float longitudinal = float((gl_VertexID & 2) >> 1);"

			"textureCoordinate = vec2(mix(startDataX, endDataX, lateral), dataY + 0.5) / textureSize(textureName, 0);"
			"vec2 eyePosition = vec2(mix(startClock, endClock, lateral), lineY + longitudinal) / vec2(2048.0, 2048.0);"
			"gl_Position = vec4(eyePosition*2.0 - vec2(1.0), 0.0, 1.0);"
		"}";

	std::string fragment_shader =
		"#version 150\n"

		"out vec4 fragColour;"
		"in vec2 textureCoordinate;"

		"uniform usampler2D textureName;"

		"void main(void) {";

	switch(modals_.input_data_type) {
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

	return std::unique_ptr<Shader>(new Shader(
		vertex_shader,
		fragment_shader + "}",
		{
			{"startDataX", 0},
			{"startClock", 1},
			{"endDataX", 2},
			{"endClock", 3},
			{"dataY", 4},
			{"lineY", 5},
		}
	));
}

std::unique_ptr<Shader> ScanTarget::conversion_shader() const {
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
		"uniform vec2 origin;"
		"uniform vec2 size;";

	std::string fragment_shader =
		"#version 150\n"

		"uniform sampler2D textureName;"

		"out vec4 fragColour;";

	if(modals_.display_type != DisplayType::RGB) {
		vertex_shader +=
			"out float compositeAngle;"
			"out float compositeAmplitude;"
			"out float oneOverCompositeAmplitude;"
		
			"uniform vec4 textureCoordinateOffsets;"
			"uniform float angleOffsets[4];"
		;
		fragment_shader +=
			"in float compositeAngle;"
			"in float compositeAmplitude;"
			"in float oneOverCompositeAmplitude;";
	}

	switch(modals_.display_type){
		case DisplayType::RGB:
		case DisplayType::CompositeMonochrome:
			vertex_shader += "out vec2 textureCoordinate;";
			fragment_shader += "in vec2 textureCoordinate;";
		break;

		case DisplayType::CompositeColour:
		case DisplayType::SVideo:
			vertex_shader +=
				"out vec2 textureCoordinates[4];"
				"out vec4 angles;";
			fragment_shader +=
				"in vec2 textureCoordinates[4];"
				"in vec4 angles;";
		break;
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
	if(modals_.display_type != DisplayType::RGB) {
		vertex_shader +=
			"compositeAngle = (mix(startCompositeAngle, endCompositeAngle, lateral) / 32.0) * 3.141592654;"
			"compositeAmplitude = lineCompositeAmplitude / 255.0;"
			"oneOverCompositeAmplitude = mix(0.0, 255.0 / lineCompositeAmplitude, step(0.01, lineCompositeAmplitude));";
	}

	// For RGB and monochrome composite, generate the single texture coordinate; otherwise generate either three
	// or four depending on the type of decoding to apply.
	switch(modals_.display_type){
		case DisplayType::RGB:
		case DisplayType::CompositeMonochrome:
			vertex_shader +=
				"textureCoordinate = vec2(mix(startClock, endClock, lateral), lineY + 0.5) / textureSize(textureName, 0);";
		break;

		case DisplayType::CompositeColour:
		case DisplayType::SVideo:
			vertex_shader +=
				"float centreClock = mix(startClock, endClock, lateral);"
				"textureCoordinates[0] = vec2(centreClock + textureCoordinateOffsets[0], lineY + 0.5) / textureSize(textureName, 0);"
				"textureCoordinates[1] = vec2(centreClock + textureCoordinateOffsets[1], lineY + 0.5) / textureSize(textureName, 0);"
				"textureCoordinates[2] = vec2(centreClock + textureCoordinateOffsets[2], lineY + 0.5) / textureSize(textureName, 0);"
				"textureCoordinates[3] = vec2(centreClock + textureCoordinateOffsets[3], lineY + 0.5) / textureSize(textureName, 0);"
				"angles = vec4("
					"compositeAngle - 2.356194490192345,"
					"compositeAngle - 0.785398163397448,"
					"compositeAngle + 0.785398163397448,"
					"compositeAngle + 2.356194490192345"
				");";
		break;
	}

	vertex_shader += "}";

	// Compose a fragment shader.
	//
	// For an RGB display ... [TODO]

	if(modals_.display_type != DisplayType::RGB) {
		fragment_shader +=
			"uniform mat3 lumaChromaToRGB;"
			"uniform mat3 rgbToLumaChroma;";
	}

	if(modals_.display_type == DisplayType::SVideo) {
		fragment_shader +=
			"vec2 svideo_sample(vec2 coordinate, float angle) {";

		switch(modals_.input_data_type) {
			case InputDataType::Luminance1:
			case InputDataType::Luminance8:
				// Easy, just copy across.
				fragment_shader += "return vec2(texture(textureName, coordinate).r, 0.0);";
			break;

			case InputDataType::PhaseLinkedLuminance8:
				fragment_shader +=
					"uint iPhase = uint((angle * 2.0 / 3.141592654) ) & 3u;"	// + phaseOffset*4.0
					"return vec2(texture(textureName, coordinate)[iPhase], 0.0);";
			break;

			case InputDataType::Luminance8Phase8:
				fragment_shader +=
					"vec2 yc = texture(textureName, coordinate).rg;"

					"float phaseOffset = 3.141592654 * 2.0 * 2.0 * yc.y;"
					"float rawChroma = step(yc.y, 0.75) * cos(angle + phaseOffset);"
					"return vec2(yc.x, rawChroma);";
			break;

			case InputDataType::Red1Green1Blue1:
			case InputDataType::Red2Green2Blue2:
			case InputDataType::Red4Green4Blue4:
			case InputDataType::Red8Green8Blue8:
				fragment_shader +=
					"vec3 colour = rgbToLumaChroma * texture(textureName, coordinate).rgb;"
					"vec2 quadrature = vec2(cos(angle), sin(angle));"
					"return vec2(colour.r, dot(quadrature, colour.gb));";
			break;
		}

		fragment_shader += "}";
	}

	if(modals_.display_type == DisplayType::CompositeMonochrome || modals_.display_type == DisplayType::CompositeColour) {
		fragment_shader +=
			"float composite_sample(vec2 coordinate, float angle) {";

		switch(modals_.input_data_type) {
			case InputDataType::Luminance1:
			case InputDataType::Luminance8:
				// Easy, just copy across.
				fragment_shader += "return texture(textureName, coordinate).r;";
			break;

			case InputDataType::PhaseLinkedLuminance8:
				fragment_shader +=
					"uint iPhase = uint((angle * 2.0 / 3.141592654) ) & 3u;"	// + phaseOffset*4.0
					"return texture(textureName, coordinate)[iPhase];";
			break;

			case InputDataType::Luminance8Phase8:
				fragment_shader +=
					"vec2 yc = texture(textureName, coordinate).rg;"

					"float phaseOffset = 3.141592654 * 2.0 * 2.0 * yc.y;"
					"float rawChroma = step(yc.y, 0.75) * cos(angle + phaseOffset);"
					"return mix(yc.x, rawChroma, compositeAmplitude);";
			break;

			case InputDataType::Red1Green1Blue1:
			case InputDataType::Red2Green2Blue2:
			case InputDataType::Red4Green4Blue4:
			case InputDataType::Red8Green8Blue8:
				fragment_shader +=
					"vec3 colour = rgbToLumaChroma * texture(textureName, coordinate).rgb;"
					"vec2 quadrature = vec2(cos(angle), sin(angle));"
					"return mix(colour.r, dot(quadrature, colour.gb), compositeAmplitude);";
			break;
		}

		fragment_shader += "}";
	}

	fragment_shader +=
		"void main(void) {"
			"vec3 fragColour3;";

	switch(modals_.display_type) {
		case DisplayType::RGB:
			fragment_shader += "fragColour3 = texture(textureName, textureCoordinate).rgb;";
		break;

		case DisplayType::SVideo:
			fragment_shader +=
				// Sample four times over, at proper angle offsets.
				"vec2 samples[4] = vec2[4]("
					"svideo_sample(textureCoordinates[0], angles[0]),"
					"svideo_sample(textureCoordinates[1], angles[1]),"
					"svideo_sample(textureCoordinates[2], angles[2]),"
					"svideo_sample(textureCoordinates[3], angles[3])"
				");"
				"vec4 chrominances = vec4("
					"samples[0].y,"
					"samples[1].y,"
					"samples[2].y,"
					"samples[3].y"
				");"

				// Split and average chrominance.
				"vec2 channels = vec2("
					"dot(cos(angles), chrominances),"
					"dot(sin(angles), chrominances)"
				") * vec2(0.25);"

				// Apply a colour space conversion to get RGB.
				"fragColour3 = lumaChromaToRGB * vec3(samples[1].x, channels);";
		break;

		case DisplayType::CompositeColour:
			fragment_shader +=
				// Sample four times over, at proper angle offsets.
				"vec4 samples = vec4("
					"composite_sample(textureCoordinates[0], angles[0]),"
					"composite_sample(textureCoordinates[1], angles[1]),"
					"composite_sample(textureCoordinates[2], angles[2]),"
					"composite_sample(textureCoordinates[3], angles[3])"
				");"

				// Compute a luminance for use if there's no colour information, now, before
				// modifying samples.
				"float mono_luminance = dot(samples.yz, vec2(0.5));"

				// Take the average to calculate luminance, then subtract that from all four samples to
				// give chrominance.
				"float luminance = dot(samples, vec4(0.25));"
				"samples -= vec4(luminance);"
				"luminance /= (1.0 - compositeAmplitude);"

				// Split and average chrominance.
				"vec2 channels = vec2("
					"dot(cos(angles), samples),"
					"dot(sin(angles), samples)"
				") * vec2(0.125 * oneOverCompositeAmplitude);"

				// Apply a colour space conversion to get RGB.
				"fragColour3 = mix("
					"lumaChromaToRGB * vec3(luminance, channels),"
					"vec3(mono_luminance),"
					"step(oneOverCompositeAmplitude, 0.01)"
				");";
		break;

		case DisplayType::CompositeMonochrome:
			fragment_shader += "fragColour3 = vec3(composite_sample(textureCoordinate, compositeAngle));";
		break;
	}

	// Apply a brightness adjustment if requested.
	if(fabs(modals_.brightness - 1.0f) > 0.05f) {
		fragment_shader += "fragColour3 = fragColour3 * " + std::to_string(modals_.brightness) + ";";
	}

	// Apply a gamma correction if required.
	if(fabs(output_gamma_ - modals_.intended_gamma) > 0.05f) {
		const float gamma_ratio = output_gamma_ / modals_.intended_gamma;
		fragment_shader += "fragColour3 = pow(fragColour3, vec3(" + std::to_string(gamma_ratio) + "));";
	}

	fragment_shader +=
			"fragColour = vec4(fragColour3, 0.64);"
		"}";

	const auto shader = new Shader(
		vertex_shader,
		fragment_shader,
		{
			{"startPoint", 0},
			{"endPoint", 1},
			{"startClock", 2},
			{"endClock", 3},
			{"lineY", 4},
			{"lineCompositeAmplitude", 5},
			{"startCompositeAngle", 6},
			{"endCompositeAngle", 7},
		}
	);

	// If this isn't an RGB or composite colour shader, set the proper colour space.
	if(modals_.display_type != DisplayType::RGB) {
		float clocks_per_angle = float(modals_.cycles_per_line) * float(modals_.colour_cycle_denominator) / float(modals_.colour_cycle_numerator);
		shader->set_uniform("textureCoordinateOffsets",
							-0.375f * clocks_per_angle,
							-0.125f * clocks_per_angle,
							+0.125f * clocks_per_angle,
							+0.375f * clocks_per_angle);

		switch(modals_.composite_colour_space) {
			case ColourSpace::YIQ: {
				const GLfloat rgbToYIQ[] = {0.299f, 0.596f, 0.211f, 0.587f, -0.274f, -0.523f, 0.114f, -0.322f, 0.312f};
				const GLfloat yiqToRGB[] = {1.0f, 1.0f, 1.0f, 0.956f, -0.272f, -1.106f, 0.621f, -0.647f, 1.703f};
				shader->set_uniform_matrix("lumaChromaToRGB", 3, false, yiqToRGB);
				shader->set_uniform_matrix("rgbToLumaChroma", 3, false, rgbToYIQ);
			} break;

			case ColourSpace::YUV: {
				const GLfloat rgbToYUV[] = {0.299f, -0.14713f, 0.615f, 0.587f, -0.28886f, -0.51499f, 0.114f, 0.436f, -0.10001f};
				const GLfloat yuvToRGB[] = {1.0f, 1.0f, 1.0f, 0.0f, -0.39465f, 2.03211f, 1.13983f, -0.58060f, 0.0f};
				shader->set_uniform_matrix("lumaChromaToRGB", 3, false, yuvToRGB);
				shader->set_uniform_matrix("rgbToLumaChroma", 3, false, rgbToYUV);
			} break;
		}
	}

	return std::unique_ptr<Shader>(shader);
}
