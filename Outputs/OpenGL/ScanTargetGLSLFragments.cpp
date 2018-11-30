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
		case ShaderType::InputScan:
		case ShaderType::ProcessedScan:
		return
			"#version 150\n"

			"uniform vec2 scale;"

			"uniform mat3 lumaChromaToRGB;"
			"uniform mat3 rgbToLumaChroma;"

			"uniform float rowHeight;"
			"uniform float processingWidth;"

			"in vec2 startPoint;"
			"in float startDataX;"
			"in float startCompositeAngle;"

			"in vec2 endPoint;"
			"in float endDataX;"
			"in float endCompositeAngle;"

			"in float dataY;"
			"in float lineY;"
			"in float compositeAmplitude;";

		case ShaderType::Line:
		return
			"#version 150\n"

			"uniform vec2 scale;"
			"uniform float rowHeight;"
			"uniform float processingWidth;"

			"in vec2 startPoint;"
			"in vec2 endPoint;"

			"in float lineY;"

			"uniform sampler2D textureName;"
			"uniform vec2 origin;"
			"uniform vec2 size;";
	}
}

std::vector<Shader::AttributeBinding> ScanTarget::attribute_bindings(ShaderType type) {
	switch(type) {
		case ShaderType::InputScan:
		case ShaderType::ProcessedScan:
		return {
			{"startPoint", 0},
			{"startDataX", 1},
			{"startCompositeAngle", 2},
			{"endPoint", 3},
			{"endDataX", 4},
			{"endCompositeAngle", 5},
			{"dataY", 6},
			{"lineY", 7},
			{"compositeAmplitude", 8},
		};

		case ShaderType::Line:
		return {
			{"startPoint", 0},
			{"endPoint", 1},
			{"lineY", 2},
		};
	}
}

std::string ScanTarget::glsl_default_vertex_shader(ShaderType type) {
	switch(type) {
		case ShaderType::InputScan:
		case ShaderType::ProcessedScan: {
			std::string result;

			if(type == ShaderType::InputScan) {
				result +=
					"out vec2 textureCoordinate;"
					"uniform usampler2D textureName;";
			} else {
				result +=
					"out vec2 textureCoordinates[15];"

					"uniform sampler2D textureName;"
					"uniform float edgeExpansion;";
			}

			result +=
				"out float compositeAngle;"
				"out float oneOverCompositeAmplitude;"

				"void main(void) {"
					"float lateral = float(gl_VertexID & 1);"
					"float longitudinal = float((gl_VertexID & 2) >> 1);"

					"compositeAngle = (mix(startCompositeAngle, endCompositeAngle, lateral) / 32.0) * 3.141592654;"
					"oneOverCompositeAmplitude = mix(0.0, 255.0 / compositeAmplitude, step(0.01, compositeAmplitude));";

			if(type == ShaderType::InputScan) {
				result +=
					"textureCoordinate = vec2(mix(startDataX, endDataX, lateral), dataY + 0.5) / textureSize(textureName, 0);"
					"vec2 eyePosition = vec2(mix(startPoint.x, endPoint.x, lateral) * processingWidth, lineY + longitudinal) / vec2(scale.x, 2048.0);";
			} else {
				result +=
					"vec2 sourcePosition = vec2(mix(startPoint.x, endPoint.x, lateral) * processingWidth, lineY + 0.5);"
					"vec2 eyePosition = (sourcePosition + vec2(0.0, longitudinal - 0.5)) / vec2(scale.x, 2048.0);"
					"sourcePosition /= vec2(scale.x, 2048.0);"

					"vec2 expansion = vec2(2.0*lateral*edgeExpansion - edgeExpansion, 0.0) / textureSize(textureName, 0);"
					"eyePosition = eyePosition + expansion;"
					"sourcePosition = sourcePosition + expansion;"

					"textureCoordinates[0] = sourcePosition + vec2(-7.0, 0.0) / textureSize(textureName, 0);"
					"textureCoordinates[1] = sourcePosition + vec2(-6.0, 0.0) / textureSize(textureName, 0);"
					"textureCoordinates[2] = sourcePosition + vec2(-5.0, 0.0) / textureSize(textureName, 0);"
					"textureCoordinates[3] = sourcePosition + vec2(-4.0, 0.0) / textureSize(textureName, 0);"
					"textureCoordinates[4] = sourcePosition + vec2(-3.0, 0.0) / textureSize(textureName, 0);"
					"textureCoordinates[5] = sourcePosition + vec2(-2.0, 0.0) / textureSize(textureName, 0);"
					"textureCoordinates[6] = sourcePosition + vec2(-1.0, 0.0) / textureSize(textureName, 0);"
					"textureCoordinates[7] = sourcePosition;"
					"textureCoordinates[8] = sourcePosition + vec2(1.0, 0.0) / textureSize(textureName, 0);"
					"textureCoordinates[9] = sourcePosition + vec2(2.0, 0.0) / textureSize(textureName, 0);"
					"textureCoordinates[10] = sourcePosition + vec2(3.0, 0.0) / textureSize(textureName, 0);"
					"textureCoordinates[11] = sourcePosition + vec2(4.0, 0.0) / textureSize(textureName, 0);"
					"textureCoordinates[12] = sourcePosition + vec2(5.0, 0.0) / textureSize(textureName, 0);"
					"textureCoordinates[13] = sourcePosition + vec2(6.0, 0.0) / textureSize(textureName, 0);"
					"textureCoordinates[14] = sourcePosition + vec2(7.0, 0.0) / textureSize(textureName, 0);"

					"eyePosition = eyePosition;";
			}

			return result +
					"gl_Position = vec4(eyePosition*2.0 - vec2(1.0), 0.0, 1.0);"
				"}";
		}

		case ShaderType::Line:
		return
			"out vec2 textureCoordinate;"

			"void main(void) {"
				"float lateral = float(gl_VertexID & 1);"
				"float longitudinal = float((gl_VertexID & 2) >> 1);"

				"textureCoordinate = vec2(lateral * processingWidth, lineY + 0.5) / vec2(1.0, textureSize(textureName, 0).y);"

				"vec2 centrePoint = mix(startPoint, endPoint, lateral) / scale;"
				"vec2 height = normalize(endPoint - startPoint).yx * (longitudinal - 0.5) * rowHeight;"
				"vec2 eyePosition = vec2(-1.0, 1.0) + vec2(2.0, -2.0) * (((centrePoint + height) - origin) / size);"
				"gl_Position = vec4(eyePosition, 0.0, 1.0);"
			"}";
	}
}

void ScanTarget::enable_vertex_attributes(ShaderType type, Shader &target) {
	switch(type) {
		case ShaderType::InputScan:
		case ShaderType::ProcessedScan:
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
			target.enable_vertex_attribute_with_pointer(
				"compositeAmplitude",
				1, GL_UNSIGNED_BYTE, GL_FALSE,
				sizeof(Scan),
				reinterpret_cast<void *>(offsetof(Scan, scan.composite_amplitude)),
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

			target.enable_vertex_attribute_with_pointer(
				"lineY",
				1, GL_UNSIGNED_SHORT, GL_FALSE,
				sizeof(Line),
				reinterpret_cast<void *>(offsetof(Line, line)),
				1);
		break;
	}
}

std::unique_ptr<Shader> ScanTarget::input_shader(InputDataType input_data_type, DisplayType display_type) {
	std::string fragment_shader =
		"#version 150\n"

		"out vec3 fragColour;"
		"in vec2 textureCoordinate;"
		"in float compositeAngle;"
		"in float oneOverCompositeAmplitude;"

		"uniform mat3 lumaChromaToRGB;"
		"uniform mat3 rgbToLumaChroma;"
		"uniform usampler2D textureName;"
		"uniform float phaseOffset;"

		"void main(void) {";

	DisplayType computed_display_type;
	switch(input_data_type) {
		case InputDataType::Luminance1:
			computed_display_type = DisplayType::CompositeMonochrome;
			fragment_shader += "fragColour = texture(textureName, textureCoordinate).rrr;";

			if(computed_display_type != display_type) {
				fragment_shader += "fragColour = clamp(fragColour, 0.0, 1.0);";
			}
		break;

		case InputDataType::Luminance8:
			computed_display_type = DisplayType::CompositeMonochrome;
			fragment_shader += "fragColour = vec3(texture(textureName, textureCoordinate).r / 255.0);";
		break;

		case InputDataType::PhaseLinkedLuminance8:
			computed_display_type = DisplayType::CompositeMonochrome;
			fragment_shader +=
				"uint iPhase = uint((compositeAngle * 2.0 / 3.141592654) + phaseOffset*4.0) & 3u;"
				"fragColour = vec3(texture(textureName, textureCoordinate)[iPhase] / 255.0);";
		break;

		case InputDataType::Luminance8Phase8:
			computed_display_type = DisplayType::SVideo;
			fragment_shader +=
				"vec2 yc = texture(textureName, textureCoordinate).rg / vec2(255.0);"

				"float phaseOffset = 3.141592654 * 2.0 * 2.0 * yc.y;"
				"float rawChroma = step(yc.y, 0.75) * cos(compositeAngle + phaseOffset);"
				"fragColour = vec3(yc.x, 0.5 + rawChroma*0.5, 0.0);";
		break;

		case InputDataType::Red1Green1Blue1:
			computed_display_type = DisplayType::RGB;
			fragment_shader +=
				"uint textureValue = texture(textureName, textureCoordinate).r;"
				"fragColour = uvec3(textureValue) & uvec3(4u, 2u, 1u);";

			if(computed_display_type != display_type) {
				fragment_shader += "fragColour = clamp(fragColour, 0.0, 1.0);";
			}
		break;

		case InputDataType::Red2Green2Blue2:
			computed_display_type = DisplayType::RGB;
			fragment_shader +=
				"uint textureValue = texture(textureName, textureCoordinate).r;"
				"fragColour = vec3(float((textureValue >> 4) & 3u), float((textureValue >> 2) & 3u), float(textureValue & 3u)) / 3.0;";
		break;

		case InputDataType::Red4Green4Blue4:
			computed_display_type = DisplayType::RGB;
			fragment_shader +=
				"uvec2 textureValue = texture(textureName, textureCoordinate).rg;"
				"fragColour = vec3(float(textureValue.r) / 15.0, float(textureValue.g & 240u) / 240.0, float(textureValue.g & 15u) / 15.0);";
		break;

		case InputDataType::Red8Green8Blue8:
			computed_display_type = DisplayType::RGB;
			fragment_shader += "fragColour = texture(textureName, textureCoordinate).rgb / vec3(255.0);";
		break;
	}

	// If the input type is RGB but the output type isn't then
	// there'll definitely be an RGB to SVideo step.
	if(computed_display_type == DisplayType::RGB && display_type != DisplayType::RGB) {
		fragment_shader +=
			"vec3 composite_colour = rgbToLumaChroma * fragColour;"
			"vec2 quadrature = vec2(cos(compositeAngle), sin(compositeAngle));"
			"fragColour = vec3(composite_colour.r, 0.5 + dot(quadrature, composite_colour.gb)*0.5, 0.0);";
	}

	// If the output type is SVideo, throw in an attempt to separate the two chrominance
	// channels here.
	if(display_type == DisplayType::SVideo) {
		if(computed_display_type != DisplayType::RGB) {
			fragment_shader +=
				"vec2 quadrature = vec2(cos(compositeAngle), sin(compositeAngle));";
		}
		fragment_shader +=
			"vec2 chroma = (((fragColour.y - 0.5)*2.0) * quadrature)*0.5 + vec2(0.5);"
			"fragColour = vec3(fragColour.x, chroma);";
	}

	// Add an SVideo to composite step if necessary.
	if(
		(display_type == DisplayType::CompositeMonochrome || display_type == DisplayType::CompositeColour) &&
		computed_display_type != DisplayType::CompositeMonochrome
	) {
		fragment_shader += "fragColour = vec3(mix(fragColour.r, 2.0*(fragColour.g - 0.5), 1.0 / oneOverCompositeAmplitude));";
	}

	return std::unique_ptr<Shader>(new Shader(
		glsl_globals(ShaderType::InputScan) + glsl_default_vertex_shader(ShaderType::InputScan),
		fragment_shader + "}",
		attribute_bindings(ShaderType::InputScan)
	));
}

SignalProcessing::FIRFilter ScanTarget::colour_filter(int colour_cycle_numerator, int colour_cycle_denominator, int processing_width, float low_cutoff, float high_cutoff) {
	const float cycles_per_expanded_line = (float(colour_cycle_numerator) / float(colour_cycle_denominator)) / (float(processing_width) / float(LineBufferWidth));
	return  SignalProcessing::FIRFilter(15, float(LineBufferWidth), cycles_per_expanded_line * low_cutoff, cycles_per_expanded_line * high_cutoff);
}

std::unique_ptr<Shader> ScanTarget::svideo_to_rgb_shader(int colour_cycle_numerator, int colour_cycle_denominator, int processing_width) {
	/*
		Composite to S-Video conversion is achieved by filtering the input signal to obtain luminance, and then subtracting that
		from the original to get chrominance.

		(Colour cycle numerator)/(Colour cycle denominator) gives the number of colour cycles in (processing_width / LineBufferWidth),
		there'll be at least four samples per colour clock and in practice at most just a shade more than 9.
	*/
	auto shader = std::unique_ptr<Shader>(new Shader(
		glsl_globals(ShaderType::ProcessedScan) + glsl_default_vertex_shader(ShaderType::ProcessedScan),
		"#version 150\n"

		"in vec2 textureCoordinates[15];"
		"uniform vec4 chromaWeights[4];"
		"uniform vec4 lumaWeights[4];"
		"uniform sampler2D textureName;"
		"uniform mat3 lumaChromaToRGB;"

		"out vec3 fragColour;"
		"void main() {"
			"vec3 samples[15] = vec3[15]("
				"texture(textureName, textureCoordinates[0]).rgb,"
				"texture(textureName, textureCoordinates[1]).rgb,"
				"texture(textureName, textureCoordinates[2]).rgb,"
				"texture(textureName, textureCoordinates[3]).rgb,"
				"texture(textureName, textureCoordinates[4]).rgb,"
				"texture(textureName, textureCoordinates[5]).rgb,"
				"texture(textureName, textureCoordinates[6]).rgb,"
				"texture(textureName, textureCoordinates[7]).rgb,"
				"texture(textureName, textureCoordinates[8]).rgb,"
				"texture(textureName, textureCoordinates[9]).rgb,"
				"texture(textureName, textureCoordinates[10]).rgb,"
				"texture(textureName, textureCoordinates[11]).rgb,"
				"texture(textureName, textureCoordinates[12]).rgb,"
				"texture(textureName, textureCoordinates[13]).rgb,"
				"texture(textureName, textureCoordinates[14]).rgb"
			");"
			"vec4 samples0[4] = vec4[4]("
				"vec4(samples[0].r, samples[1].r, samples[2].r, samples[3].r),"
				"vec4(samples[4].r, samples[5].r, samples[6].r, samples[7].r),"
				"vec4(samples[8].r, samples[9].r, samples[10].r, samples[11].r),"
				"vec4(samples[12].r, samples[13].r, samples[14].r, 0.0)"
			");"
			"vec4 samples1[4] = vec4[4]("
				"vec4(samples[0].g, samples[1].g, samples[2].g, samples[3].g),"
				"vec4(samples[4].g, samples[5].g, samples[6].g, samples[7].g),"
				"vec4(samples[8].g, samples[9].g, samples[10].g, samples[11].g),"
				"vec4(samples[12].g, samples[13].g, samples[14].g, 0.0)"
			");"
			"vec4 samples2[4] = vec4[4]("
				"vec4(samples[0].b, samples[1].b, samples[2].b, samples[3].b),"
				"vec4(samples[4].b, samples[5].b, samples[6].b, samples[7].b),"
				"vec4(samples[8].b, samples[9].b, samples[10].b, samples[11].b),"
				"vec4(samples[12].b, samples[13].b, samples[14].b, 0.0)"
			");"
			"float channel0 = dot(lumaWeights[0], samples0[0]) + dot(lumaWeights[1], samples0[1]) + dot(lumaWeights[2], samples0[2]) + dot(lumaWeights[3], samples0[3]);"
			"float channel1 = dot(chromaWeights[0], samples1[0]) + dot(chromaWeights[1], samples1[1]) + dot(chromaWeights[2], samples1[2]) + dot(chromaWeights[3], samples1[3]);"
			"float channel2 = dot(chromaWeights[0], samples2[0]) + dot(chromaWeights[1], samples2[1]) + dot(chromaWeights[2], samples2[2]) + dot(chromaWeights[3], samples2[3]);"
			"vec2 chroma = vec2(channel1, channel2)*2.0 - vec2(1.0);"
			"fragColour = lumaChromaToRGB * vec3(channel0, chroma);"
		"}",
		attribute_bindings(ShaderType::ProcessedScan)
	));

	auto chroma_coefficients = colour_filter(colour_cycle_numerator, colour_cycle_denominator, processing_width, 0.0f, 0.25f).get_coefficients();
	chroma_coefficients.push_back(0.0f);
	shader->set_uniform("chromaWeights", 4, 4, chroma_coefficients.data());

	auto luma_coefficients = colour_filter(colour_cycle_numerator, colour_cycle_denominator, processing_width, 0.0f, 0.15f).get_coefficients();
	luma_coefficients.push_back(0.0f);
	shader->set_uniform("lumaWeights", 4, 4, luma_coefficients.data());

	shader->set_uniform("edgeExpansion", 0);

	return shader;
}

std::unique_ptr<Shader> ScanTarget::composite_to_svideo_shader(int colour_cycle_numerator, int colour_cycle_denominator, int processing_width) {
	auto shader = std::unique_ptr<Shader>(new Shader(
		glsl_globals(ShaderType::ProcessedScan) + glsl_default_vertex_shader(ShaderType::ProcessedScan),
		"#version 150\n"

		"in vec2 textureCoordinates[15];"
		"in float compositeAngle;"
		"in float oneOverCompositeAmplitude;"

		"uniform vec4 lumaWeights[4];"
		"uniform sampler2D textureName;"

		"out vec3 fragColour;"
		"void main() {"
			"vec4 samples[4] = vec4[4]("
				"vec4(texture(textureName, textureCoordinates[0]).r, texture(textureName, textureCoordinates[1]).r, texture(textureName, textureCoordinates[2]).r, texture(textureName, textureCoordinates[3]).r),"
				"vec4(texture(textureName, textureCoordinates[4]).r, texture(textureName, textureCoordinates[5]).r, texture(textureName, textureCoordinates[6]).r, texture(textureName, textureCoordinates[7]).r),"
				"vec4(texture(textureName, textureCoordinates[8]).r, texture(textureName, textureCoordinates[9]).r, texture(textureName, textureCoordinates[10]).r, texture(textureName, textureCoordinates[11]).r),"
				"vec4(texture(textureName, textureCoordinates[12]).r, texture(textureName, textureCoordinates[13]).r, texture(textureName, textureCoordinates[14]).r, 0.0)"
			");"
			"float luma = dot(lumaWeights[0], samples[0]) + dot(lumaWeights[1], samples[1]) + dot(lumaWeights[2], samples[2]) + dot(lumaWeights[3], samples[3]);"
			"vec2 quadrature = vec2(cos(compositeAngle), sin(compositeAngle));"
			"vec2 chroma = ((samples[1].a - luma) * oneOverCompositeAmplitude)*quadrature;"
			"fragColour = vec3(samples[1].a, chroma*0.5 + vec2(0.5));"
		"}",
		attribute_bindings(ShaderType::ProcessedScan)
	));

	auto luma_low = colour_filter(colour_cycle_numerator, colour_cycle_denominator, processing_width, 0.0f, 0.9f);
	auto luma_coefficients = luma_low.get_coefficients();
	luma_coefficients.push_back(0.0f);
	shader->set_uniform("lumaWeights", 4, 4, luma_coefficients.data());

	shader->set_uniform("edgeExpansion", 0);

	return shader;
}

