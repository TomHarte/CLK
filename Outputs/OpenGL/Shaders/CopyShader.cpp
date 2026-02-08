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

in vec2 position;
out vec2 coordinate;

void main(void) {
	coordinate = (position + vec2(1.0)) / vec2(2.0);
	gl_Position = vec4(
		position,
		0.0,
		1.0
	);
}

)glsl";


constexpr char fragment_shader[] = R"glsl(

uniform sampler2D source;
uniform float brightness;
uniform float gamma;

in vec2 coordinate;

out vec4 outputColour;

void main(void) {
	outputColour = texture(source, coordinate);

#ifdef APPLY_BRIGHTNESS
	outputColour *= brightness;
#endif

#ifdef APPLY_GAMMA
	outputColour = vec4(
		pow(outputColour.r, gamma),
		pow(outputColour.g, gamma),
		pow(outputColour.b, gamma),
		1.0
	);
#endif
}

)glsl";

}

using namespace Outputs::Display;

OpenGL::CopyShader::CopyShader(
	const OpenGL::API api,
	std::optional<float> brightness,
	std::optional<float> gamma
) {
	// Establish shader.
	std::string prefix;

	if(brightness) {
		prefix += "#define APPLY_BRIGHTNESS\n";
	}
	if(gamma) {
		prefix += "#define APPLY_GAMMA\n";
	}

	shader_ = OpenGL::Shader(
		api,
		prefix + vertex_shader,
		prefix + fragment_shader
	);

	if(brightness) {
		shader_.set_uniform("brightness", *brightness);
	}
	if(gamma) {
		shader_.set_uniform("gamma", *gamma);
	}

	// Establish a vertex array, to make the shader formally safe to call,
	// regardless of OpenGL version.
	static constexpr float corners[] = {
		-1.0f, -1.0f,
		-1.0f, 1.0f,
		1.0f, -1.0f,
		1.0f, 1.0f
	};
	vertices_ = VertexArray(corners);
	vertices_.bind_all();
	test_gl([&]{ glBufferData(GL_ARRAY_BUFFER, sizeof(corners), corners, GL_STATIC_DRAW); });
	shader_.enable_vertex_attribute_with_pointer(
		"position",
		2,
		GL_FLOAT,
		GL_FALSE,
		0,
		nullptr,
		0
	);
}

void OpenGL::CopyShader::perform(const GLenum source) {
	shader_.bind();
	if(source_ != source) {
		source_ = source;
		shader_.set_uniform("source", GLint(source_ - GL_TEXTURE0));
	}

	vertices_.bind();
	test_gl([&]{ glDrawArrays(GL_TRIANGLE_STRIP, 0, 4); });
}
