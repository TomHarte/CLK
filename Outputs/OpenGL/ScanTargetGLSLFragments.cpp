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
			"in float startClock;"

			"in vec2 endPoint;"
			"in float endDataX;"
			"in float endCompositeAngle;"
			"in float endClock;"

			"in float dataY;"
			"in float lineY;"
			"in float compositeAmplitude;";

		case ShaderType::Line:
		return "";
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
			{"startClock", 3},
			{"endPoint", 4},
			{"endDataX", 5},
			{"endCompositeAngle", 6},
			{"endClock", 7},
			{"dataY", 8},
			{"lineY", 9},
			{"compositeAmplitude", 10},
		};

		case ShaderType::Line:
		return {
			{"startPoint", 0},
			{"endPoint", 1},
			{"startClock", 2},
			{"endClock", 3},
			{"lineY", 4},
			{"lineCompositeAmplitude", 5},
			{"startCompositeAngle", 6},
			{"endCompositeAngle", 7},
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
					"out vec2 chromaCoordinates[2];"

					"uniform sampler2D textureName;"
					"uniform float chromaOffset;"
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
					"vec2 eyePosition = vec2(mix(startClock, endClock, lateral), lineY + longitudinal) / vec2(2048.0, 2048.0);";
			} else {
				result +=
					"vec2 sourcePosition = vec2(mix(startPoint.x, endPoint.x, lateral) * processingWidth, lineY + 0.5);"
					"vec2 eyePosition = (sourcePosition + vec2(0.0, longitudinal - 0.5)) / vec2(scale.x, 2048.0);"
					"sourcePosition /= vec2(scale.x, 2048.0);"

//					"vec2 expansion = vec2(edgeExpansion, 0.0) / textureSize(textureName, 0);"
//					"eyePosition = eyePosition + expansion;"
//					"sourcePosition = sourcePosition + expansion;"

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

					"chromaCoordinates[0] = sourcePosition + vec2(chromaOffset, 0.0);"
					"chromaCoordinates[1] = sourcePosition - vec2(chromaOffset, 0.0);"

					"eyePosition = eyePosition;";
			}

			return result +
					"gl_Position = vec4(eyePosition*2.0 - vec2(1.0), 0.0, 1.0);"
				"}";
		}

		case ShaderType::Line:
		return
			"out vec2 textureCoordinates[4];"

			"out float compositeAngle;"
			"out float oneOverCompositeAmplitude;"

			"void main(void) {"
				"float lateral = float(gl_VertexID & 1);"
				"float longitudinal = float((gl_VertexID & 2) >> 1);"

				"textureCoordinates[0] = vec2(mix(startClock, endClock, lateral), lineY + 0.5) / textureSize(textureName, 0);"

				"compositeAngle = (mix(startCompositeAngle, endCompositeAngle, lateral) / 32.0) * 3.141592654;"
				"oneOverCompositeAmplitude = mix(0.0, 255.0 / compositeAmplitude, step(0.01, compositeAmplitude));"

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
				target.enable_vertex_attribute_with_pointer(
					prefix + "Clock",
					1, GL_UNSIGNED_SHORT, GL_FALSE,
					sizeof(Scan),
					reinterpret_cast<void *>(offsetof(Scan, scan.end_points[c].cycles_since_end_of_horizontal_retrace)),
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

				target.enable_vertex_attribute_with_pointer(
					prefix + "Clock",
					1, GL_UNSIGNED_SHORT, GL_FALSE,
					sizeof(Line),
					reinterpret_cast<void *>(offsetof(Line, end_points[c].cycles_since_end_of_horizontal_retrace)),
					1);

				target.enable_vertex_attribute_with_pointer(
					prefix + "CompositeAngle",
					1, GL_UNSIGNED_SHORT, GL_FALSE,
					sizeof(Line),
					reinterpret_cast<void *>(offsetof(Line, end_points[c].composite_angle)),
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
}

std::unique_ptr<Shader> ScanTarget::composition_shader(InputDataType input_data_type) {
/*	std::string fragment_shader =
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
	}*/

	// If the input type is RGB but the output type isn't then
	// there'll definitely be an RGB to SVideo step.
//	if(computed_display_type == DisplayType::RGB && display_type != DisplayType::RGB) {
//		fragment_shader +=
//			"vec3 composite_colour = rgbToLumaChroma * fragColour;"
//			"vec2 quadrature = vec2(cos(compositeAngle), sin(compositeAngle));"
//			"fragColour = vec3(composite_colour.r, 0.5 + dot(quadrature, composite_colour.gb)*0.5, 0.0);";
//	}

	// If the output type is SVideo, throw in an attempt to separate the two chrominance
	// channels here.
//	if(display_type == DisplayType::SVideo) {
//		if(computed_display_type != DisplayType::RGB) {
//			fragment_shader +=
//				"vec2 quadrature = vec2(cos(compositeAngle), sin(compositeAngle));";
//		}
//		fragment_shader +=
//			"vec2 chroma = (((fragColour.y - 0.5)*2.0) * quadrature)*0.5 + vec2(0.5);"
//			"fragColour = vec3(fragColour.x, chroma);";
//	}

	// Add an SVideo to composite step if necessary.
//	if(
//		(display_type == DisplayType::CompositeMonochrome || display_type == DisplayType::CompositeColour) &&
//		computed_display_type != DisplayType::CompositeMonochrome
//	) {
//		fragment_shader += "fragColour = vec3(mix(fragColour.r, 2.0*(fragColour.g - 0.5), 1.0 / oneOverCompositeAmplitude));";
//	}


	std::string fragment_shader =
		"#version 150\n"

		"out vec4 fragColour;"
		"in vec2 textureCoordinate;"

		"uniform usampler2D textureName;"

		"void main(void) {";

	switch(input_data_type) {
		case InputDataType::Luminance1:
			fragment_shader += "fragColour = texture(textureName, textureCoordinate).rrrr;";
		break;

		case InputDataType::Luminance8:
			fragment_shader += "fragColour = texture(textureName, textureCoordinate).rrrr / vec4(255.0);";
		break;

		case InputDataType::PhaseLinkedLuminance8:
		case InputDataType::Luminance8Phase8:
		case InputDataType::Red8Green8Blue8:
			fragment_shader += "fragColour = texture(textureName, textureCoordinate) / vec4(255.0);";
		break;

		case InputDataType::Red1Green1Blue1:
			fragment_shader += "fragColour = vec4(texture(textureName, textureCoordinate).rrr & uvec3(4u, 2u, 1u), 1.0);";
		break;

		case InputDataType::Red2Green2Blue2:
			fragment_shader +=
				"uint textureValue = texture(textureName, textureCoordinate).r;"
				"fragColour = vec4(float((textureValue >> 4) & 3u), float((textureValue >> 2) & 3u), float(textureValue & 3u), 3.0) / 3.0;";
		break;

		case InputDataType::Red4Green4Blue4:
			fragment_shader +=
				"uvec2 textureValue = texture(textureName, textureCoordinate).rg;"
				"fragColour = vec4(float(textureValue.r) / 15.0, float(textureValue.g & 240u) / 240.0, float(textureValue.g & 15u) / 15.0, 1.0);";
		break;
	}

	return std::unique_ptr<Shader>(new Shader(
		glsl_globals(ShaderType::InputScan) + glsl_default_vertex_shader(ShaderType::InputScan),
		fragment_shader + "}",
		attribute_bindings(ShaderType::InputScan)
	));
}

std::unique_ptr<Shader> ScanTarget::conversion_shader(InputDataType input_data_type, DisplayType display_type, ColourSpace colour_space) {
	display_type = DisplayType::CompositeMonochrome;	// Just a test.

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
		"uniform float processingWidth;"

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

	if(display_type != DisplayType::RGB) {
		vertex_shader +=
			"out float compositeAngle;"
			"out float compositeAmplitude;"
			"out float oneOverCompositeAmplitude;";
		fragment_shader +=
			"in float compositeAngle;"
			"in float compositeAmplitude;"
			"in float oneOverCompositeAmplitude;";
	}

	switch(display_type){
		case DisplayType::RGB:
		case DisplayType::CompositeMonochrome:
			vertex_shader += "out vec2 textureCoordinate;";
			fragment_shader += "in vec2 textureCoordinate;";
		break;

		case DisplayType::CompositeColour:
			vertex_shader += "out vec2 textureCoordinates[4];";
			fragment_shader += "in vec2 textureCoordinates[4];";
		break;

		case DisplayType::SVideo:
			vertex_shader += "out vec2 textureCoordinates[3];";
			fragment_shader += "in vec2 textureCoordinates[3];";
		break;
	}

	// Add the code to generate a proper output position; this applies to all display types.
	vertex_shader +=
			"void main(void) {"
				"float lateral = float(gl_VertexID & 1);"
				"float longitudinal = float((gl_VertexID & 2) >> 1);"
				"vec2 centrePoint = mix(startPoint, endPoint, lateral) / scale;"
				"vec2 height = normalize(endPoint - startPoint).yx * (longitudinal - 0.5) * rowHeight;"
				"vec2 eyePosition = vec2(-1.0, 1.0) + vec2(2.0, -2.0) * (((centrePoint + height) - origin) / size);"
				"gl_Position = vec4(eyePosition, 0.0, 1.0);";

	// For everything other than RGB, calculate the two composite outputs.
	if(display_type != DisplayType::RGB) {
		vertex_shader +=
				"compositeAngle = (mix(startCompositeAngle, endCompositeAngle, lateral) / 32.0) * 3.141592654;"
				"compositeAmplitude = lineCompositeAmplitude / 255.0;"
				"oneOverCompositeAmplitude = mix(0.0, 255.0 / lineCompositeAmplitude, step(0.01, lineCompositeAmplitude));";
	}

	// For RGB and monochrome composite, generate the single texture coordinate; otherwise generate either three
	// or four depending on the type of decoding to apply.
	switch(display_type){
		case DisplayType::RGB:
		case DisplayType::CompositeMonochrome:
			vertex_shader +=
				"textureCoordinate = vec2(mix(startClock, endClock, lateral), lineY + 0.5) / textureSize(textureName, 0);";
		break;

		case DisplayType::CompositeColour:
			// TODO
		break;

		case DisplayType::SVideo:
			// TODO
		break;
	}

	vertex_shader += "}";

	// Compose a fragment shader.
	//
	// For an RGB display ... [TODO]

	if(display_type != DisplayType::RGB) {
		fragment_shader +=
			"uniform mat3 lumaChromaToRGB;"
			"uniform mat3 rgbToLumaChroma;";
	}

	fragment_shader +=
		"void main(void) {"
			"vec3 fragColour3;";

	switch(display_type) {
		case DisplayType::RGB:
			// Easy, just copy across.
			fragment_shader += "fragColour3 = texture(textureName, textureCoordinate).rgb;";
		break;

		case DisplayType::SVideo:
			// TODO
		break;

		case DisplayType::CompositeColour:
			// TODO
		break;

		case DisplayType::CompositeMonochrome: {
			switch(input_data_type) {
				case InputDataType::Luminance1:
				case InputDataType::Luminance8:
					// Easy, just copy across.
					fragment_shader += "fragColour3 = texture(textureName, textureCoordinate).rgb;";
				break;

				case InputDataType::PhaseLinkedLuminance8:
					fragment_shader +=
						"uint iPhase = uint((compositeAngle * 2.0 / 3.141592654) ) & 3u;"	// + phaseOffset*4.0
						"fragColour3 = vec3(texture(textureName, textureCoordinate)[iPhase]);";
				break;

				case InputDataType::Luminance8Phase8:
					fragment_shader +=
						"vec2 yc = texture(textureName, textureCoordinate).rg;"

						"float phaseOffset = 3.141592654 * 2.0 * 2.0 * yc.y;"
						"float rawChroma = step(yc.y, 0.75) * cos(compositeAngle + phaseOffset);"
						"float level = mix(yc.x, yc.y * rawChroma, compositeAmplitude);"
						"fragColour3 = vec3(level);";
				break;

				case InputDataType::Red1Green1Blue1:
				case InputDataType::Red2Green2Blue2:
				case InputDataType::Red4Green4Blue4:
				case InputDataType::Red8Green8Blue8:
					fragment_shader +=
						"vec3 colour = rgbToLumaChroma * texture(textureName, textureCoordinate).rgb;"
						"vec2 quadrature = vec2(cos(compositeAngle), sin(compositeAngle));"
						"float level = mix(colour.r, dot(quadrature, colour.gb), compositeAmplitude);"
						"fragColour3 = vec3(level);";
				break;
			}
		} break;
	}

	fragment_shader +=
			"fragColour = vec4(fragColour3, 0.64);"
		"}";

	// TODO gamma and range corrections.

	const auto shader = new Shader(
		vertex_shader,
		fragment_shader,
		attribute_bindings(ShaderType::Line)
	);

	// If this isn't an RGB or composite colour shader, set the proper colour space.
	if(display_type != DisplayType::RGB) {
		switch(colour_space) {
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

//
//SignalProcessing::FIRFilter ScanTarget::colour_filter(int colour_cycle_numerator, int colour_cycle_denominator, int processing_width, float low_cutoff, float high_cutoff) {
//	const float cycles_per_expanded_line = (float(colour_cycle_numerator) / float(colour_cycle_denominator)) / (float(processing_width) / float(LineBufferWidth));
//	return  SignalProcessing::FIRFilter(15, float(LineBufferWidth), cycles_per_expanded_line * low_cutoff, cycles_per_expanded_line * high_cutoff);
//}
//
//std::unique_ptr<Shader> ScanTarget::svideo_to_rgb_shader(int colour_cycle_numerator, int colour_cycle_denominator, int processing_width) {
//	/*
//		Composite to S-Video conversion is achieved by filtering the input signal to obtain luminance, and then subtracting that
//		from the original to get chrominance.
//
//		(Colour cycle numerator)/(Colour cycle denominator) gives the number of colour cycles in (processing_width / LineBufferWidth),
//		there'll be at least four samples per colour clock and in practice at most just a shade more than 9.
//	*/
//	auto shader = std::unique_ptr<Shader>(new Shader(
//		glsl_globals(ShaderType::ProcessedScan) + glsl_default_vertex_shader(ShaderType::ProcessedScan),
//		"#version 150\n"
//
//		"in vec2 textureCoordinates[15];"
//		"in vec2 chromaCoordinates[2];"
//		"in float compositeAngle;"
//
////		"uniform vec4 chromaWeights[4];"
////		"uniform vec4 lumaWeights[4];"
//		"uniform sampler2D textureName;"
//		"uniform mat3 lumaChromaToRGB;"
//
//		"out vec3 fragColour;"
//		"void main() {"
//			"vec2 angles = vec2(compositeAngle - 1.570795827, compositeAngle + 1.570795827);"
//
//			"vec2 sines = sin(angles) * vec2(0.5) + vec2(0.5);"
//			"vec2 coses = cos(angles);"
//			"float denominator = sines.y * coses.x - sines.x * coses.y;"
//
//			"vec2 samples = vec2(texture(textureName, chromaCoordinates[0]).g, texture(textureName, chromaCoordinates[1]).g);"
//
//			"float channel1 = (samples.x * sines.x - samples.y * sines.y) / denominator;"
//			"float channel2 = (samples.x * coses.x - samples.y * coses.y) / denominator;"
//
////			"fragColour = lumaChromaToRGB * vec3(texture(textureName, textureCoordinates[7]).r, channel1, channel2);"
//			"fragColour = vec3(sines.x + sines.y, 0.0, 0.0);"
//			//, 0.0);"
//
////			"fragColour = lumaChromaToRGB * vec3(texture(textureName, textureCoordinates[7]).g, 0.0, 0.0);"
////			"fragColour = vec3(0.5);"
///*			"vec3 samples[15] = vec3[15]("
//				"texture(textureName, textureCoordinates[0]).rgb,"
//				"texture(textureName, textureCoordinates[1]).rgb,"
//				"texture(textureName, textureCoordinates[2]).rgb,"
//				"texture(textureName, textureCoordinates[3]).rgb,"
//				"texture(textureName, textureCoordinates[4]).rgb,"
//				"texture(textureName, textureCoordinates[5]).rgb,"
//				"texture(textureName, textureCoordinates[6]).rgb,"
//				"texture(textureName, textureCoordinates[7]).rgb,"
//				"texture(textureName, textureCoordinates[8]).rgb,"
//				"texture(textureName, textureCoordinates[9]).rgb,"
//				"texture(textureName, textureCoordinates[10]).rgb,"
//				"texture(textureName, textureCoordinates[11]).rgb,"
//				"texture(textureName, textureCoordinates[12]).rgb,"
//				"texture(textureName, textureCoordinates[13]).rgb,"
//				"texture(textureName, textureCoordinates[14]).rgb"
//			");"
//			"vec4 samples0[4] = vec4[4]("
//				"vec4(samples[0].r, samples[1].r, samples[2].r, samples[3].r),"
//				"vec4(samples[4].r, samples[5].r, samples[6].r, samples[7].r),"
//				"vec4(samples[8].r, samples[9].r, samples[10].r, samples[11].r),"
//				"vec4(samples[12].r, samples[13].r, samples[14].r, 0.0)"
//			");"
//			"vec4 samples1[4] = vec4[4]("
//				"vec4(samples[0].g, samples[1].g, samples[2].g, samples[3].g),"
//				"vec4(samples[4].g, samples[5].g, samples[6].g, samples[7].g),"
//				"vec4(samples[8].g, samples[9].g, samples[10].g, samples[11].g),"
//				"vec4(samples[12].g, samples[13].g, samples[14].g, 0.0)"
//			");"
//			"vec4 samples2[4] = vec4[4]("
//				"vec4(samples[0].b, samples[1].b, samples[2].b, samples[3].b),"
//				"vec4(samples[4].b, samples[5].b, samples[6].b, samples[7].b),"
//				"vec4(samples[8].b, samples[9].b, samples[10].b, samples[11].b),"
//				"vec4(samples[12].b, samples[13].b, samples[14].b, 0.0)"
//			");"
//			"float channel0 = dot(lumaWeights[0], samples0[0]) + dot(lumaWeights[1], samples0[1]) + dot(lumaWeights[2], samples0[2]) + dot(lumaWeights[3], samples0[3]);"
//			"float channel1 = dot(chromaWeights[0], samples1[0]) + dot(chromaWeights[1], samples1[1]) + dot(chromaWeights[2], samples1[2]) + dot(chromaWeights[3], samples1[3]);"
//			"float channel2 = dot(chromaWeights[0], samples2[0]) + dot(chromaWeights[1], samples2[1]) + dot(chromaWeights[2], samples2[2]) + dot(chromaWeights[3], samples2[3]);"
//			"vec2 chroma = vec2(channel1, channel2)*2.0 - vec2(1.0);"
//			"fragColour = lumaChromaToRGB * vec3(channel0, chroma);"*/
//		"}",
//		attribute_bindings(ShaderType::ProcessedScan)
//	));
//
//	const float cycles_per_expanded_line = (float(colour_cycle_numerator) / float(colour_cycle_denominator)) / (float(processing_width) / float(LineBufferWidth));
//	const float chroma_offset = 0.25f / cycles_per_expanded_line;
//	shader->set_uniform("chromaOffset", chroma_offset);
//
////	auto chroma_coefficients = colour_filter(colour_cycle_numerator, colour_cycle_denominator, processing_width, 0.0f, 0.25f).get_coefficients();
////	chroma_coefficients.push_back(0.0f);
////	shader->set_uniform("chromaWeights", 4, 4, chroma_coefficients.data());
////
////	auto luma_coefficients = colour_filter(colour_cycle_numerator, colour_cycle_denominator, processing_width, 0.0f, 0.15f).get_coefficients();
////	luma_coefficients.push_back(0.0f);
////	shader->set_uniform("lumaWeights", 4, 4, luma_coefficients.data());
//
//	shader->set_uniform("edgeExpansion", 20);
//
//	return shader;
//}
//
//std::unique_ptr<Shader> ScanTarget::composite_to_svideo_shader(int colour_cycle_numerator, int colour_cycle_denominator, int processing_width) {
//	auto shader = std::unique_ptr<Shader>(new Shader(
//		glsl_globals(ShaderType::ProcessedScan) + glsl_default_vertex_shader(ShaderType::ProcessedScan),
//		"#version 150\n"
//
//		"in vec2 textureCoordinates[15];"
//		"in float compositeAngle;"
//		"in float oneOverCompositeAmplitude;"
//
//		"uniform vec4 lumaWeights[4];"
//		"uniform sampler2D textureName;"
//
//		"out vec3 fragColour;"
//		"void main() {"
//			"vec4 samples[4] = vec4[4]("
//				"vec4(texture(textureName, textureCoordinates[0]).r, texture(textureName, textureCoordinates[1]).r, texture(textureName, textureCoordinates[2]).r, texture(textureName, textureCoordinates[3]).r),"
//				"vec4(texture(textureName, textureCoordinates[4]).r, texture(textureName, textureCoordinates[5]).r, texture(textureName, textureCoordinates[6]).r, texture(textureName, textureCoordinates[7]).r),"
//				"vec4(texture(textureName, textureCoordinates[8]).r, texture(textureName, textureCoordinates[9]).r, texture(textureName, textureCoordinates[10]).r, texture(textureName, textureCoordinates[11]).r),"
//				"vec4(texture(textureName, textureCoordinates[12]).r, texture(textureName, textureCoordinates[13]).r, texture(textureName, textureCoordinates[14]).r, 0.0)"
//			");"
//			"float luma = dot(lumaWeights[0], samples[0]) + dot(lumaWeights[1], samples[1]) + dot(lumaWeights[2], samples[2]) + dot(lumaWeights[3], samples[3]);"
//			"vec2 quadrature = vec2(cos(compositeAngle), sin(compositeAngle));"
//			"vec2 chroma = ((samples[1].a - luma) * oneOverCompositeAmplitude)*quadrature;"
//			"fragColour = vec3(samples[1].a, chroma*0.5 + vec2(0.5));"
//		"}",
//		attribute_bindings(ShaderType::ProcessedScan)
//	));
//
//	auto luma_low = colour_filter(colour_cycle_numerator, colour_cycle_denominator, processing_width, 0.0f, 0.9f);
//	auto luma_coefficients = luma_low.get_coefficients();
//	luma_coefficients.push_back(0.0f);
//	shader->set_uniform("lumaWeights", 4, 4, luma_coefficients.data());
//
//	shader->set_uniform("edgeExpansion", 10);
//
//	return shader;
//}
//
