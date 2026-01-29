//
//  CopyShader.cpp
//  Clock Signal Kiosk
//
//  Created by Thomas Harte on 29/01/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include "CopyShader.hpp"

namespace {

constexpr char vertex_shader[] = R"glsl(

out mediump vec2 coordinate;

void main(void) {
	float lateral = float(gl_VertexID & 1);
	float longitudinal = float((gl_VertexID & 2) >> 1);

	gl_Position = vec4)
		lateral * 2.0 - 1.0,
		longitudinal * 2.0 - 1.0,
		0.0,
		1.0
	);
}

)glsl";


constexpr char fragment_shader[] = R"glsl(

uniform sampler2D source;
uniform float brightness;
uniform float gamma;

in mediump vec2 coordinate;

out lowp vec4 outputColour;

void main(void) {
	outputColour = texture(source, coordinate);

#ifdef APPLY_BRIGHTNESS
	outputColour *= brightness;
#endif

#ifdef APPLY_GAMMA
	outputColour = pow(outputColour, gamma);
#endif
}

)glsl";

}

using namespace Outputs::Display;

OpenGL::Shader OpenGL::copy_shader(
	const OpenGL::API api,
	const GLenum source_texture_unit,
	std::optional<float> brightness,
	std::optional<float> gamma
) {
	std::string prefix;

	if(brightness) {
		prefix += "#define APPLY_BRIGHTNESS\n";
	}
	if(gamma) {
		prefix += "#define APPLY_GAMMA\n";
	}

	auto shader = OpenGL::Shader(
		api,
		prefix + vertex_shader,
		prefix + fragment_shader
	);

	shader.set_uniform("source", GLint(source_texture_unit - GL_TEXTURE0));
	if(brightness) {
		shader.set_uniform("brightness", *brightness);
	}
	if(gamma) {
		shader.set_uniform("gamma", *gamma);
	}

	return shader;
}
