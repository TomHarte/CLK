//
//  TextureTarget.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/02/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "TextureTarget.hpp"

#include <cstdlib>
#include <vector>

using namespace OpenGL;

TextureTarget::TextureTarget(GLsizei width, GLsizei height, GLenum texture_unit, GLint mag_filter) :
		width_(width),
		height_(height),
		texture_unit_(texture_unit) {
	glGenFramebuffers(1, &framebuffer_);
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);

	expanded_width_ = 1;
	while(expanded_width_ < width)		expanded_width_ <<= 1;
	expanded_height_ = 1;
	while(expanded_height_ < height)	expanded_height_ <<= 1;

	glGenTextures(1, &texture_);
	bind_texture();

	std::vector<uint8_t> blank_buffer(static_cast<size_t>(expanded_width_ * expanded_height_ * 4), 0);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, static_cast<GLsizei>(expanded_width_), static_cast<GLsizei>(expanded_height_), 0, GL_RGBA, GL_UNSIGNED_BYTE, blank_buffer.data());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_, 0);

	if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		throw ErrorFramebufferIncomplete;
}

TextureTarget::~TextureTarget() {
	glDeleteFramebuffers(1, &framebuffer_);
	glDeleteTextures(1, &texture_);
	if(drawing_vertex_array_) glDeleteVertexArrays(1, &drawing_vertex_array_);
	if(drawing_array_buffer_) glDeleteBuffers(1, &drawing_array_buffer_);
}

void TextureTarget::bind_framebuffer() {
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
	glViewport(0, 0, width_, height_);
}

void TextureTarget::bind_texture() {
	glActiveTexture(texture_unit_);
	glBindTexture(GL_TEXTURE_2D, texture_);
}

void TextureTarget::draw(float aspect_ratio, float colour_threshold) {
	if(!pixel_shader_) {
		const char *vertex_shader =
			"#version 150\n"

			"in vec2 texCoord;"
			"in vec2 position;"

			"out vec2 texCoordVarying;"

			"void main(void)"
			"{"
				"texCoordVarying = texCoord;"
				"gl_Position = vec4(position, 0.0, 1.0);"
			"}";
		const char *fragment_shader =
			"#version 150\n"

			"in vec2 texCoordVarying;"

			"uniform sampler2D texID;"
			"uniform float threshold;"

			"out vec4 fragColour;"

			"void main(void)"
			"{"
				"fragColour = clamp(texture(texID, texCoordVarying), threshold, 1.0);"
			"}";
		pixel_shader_.reset(new Shader(vertex_shader, fragment_shader));
		pixel_shader_->bind();

		glGenVertexArrays(1, &drawing_vertex_array_);
		glGenBuffers(1, &drawing_array_buffer_);

		glBindVertexArray(drawing_vertex_array_);
		glBindBuffer(GL_ARRAY_BUFFER, drawing_array_buffer_);

		GLint position_attribute	= pixel_shader_->get_attrib_location("position");
		GLint tex_coord_attribute	= pixel_shader_->get_attrib_location("texCoord");

		glEnableVertexAttribArray(static_cast<GLuint>(position_attribute));
		glEnableVertexAttribArray(static_cast<GLuint>(tex_coord_attribute));

		const GLsizei vertex_stride = 4 * sizeof(GLfloat);
		glVertexAttribPointer((GLuint)position_attribute,	2, GL_FLOAT,	GL_FALSE,	vertex_stride,	(void *)0);
		glVertexAttribPointer((GLuint)tex_coord_attribute,	2, GL_FLOAT,	GL_FALSE,	vertex_stride,	(void *)(2 * sizeof(GLfloat)));

		GLint texIDUniform = pixel_shader_->get_uniform_location("texID");
		glUniform1i(texIDUniform, static_cast<GLint>(texture_unit_ - GL_TEXTURE0));

		threshold_uniform_ = pixel_shader_->get_uniform_location("threshold");
	}

	if(set_aspect_ratio_ != aspect_ratio) {
		set_aspect_ratio_ = aspect_ratio;
		float buffer[4*4];

		// establish texture coordinates
		buffer[2] = 0.0f;
		buffer[3] = 0.0f;
		buffer[6] = 0.0f;
		buffer[7] = static_cast<float>(height_) / static_cast<float>(expanded_height_);
		buffer[10] = static_cast<float>(width_) / static_cast<float>(expanded_width_);
		buffer[11] = 0.0f;
		buffer[14] = buffer[10];
		buffer[15] = buffer[7];

		// determine positions; rule is to keep the same height and centre
		float internal_aspect_ratio = static_cast<float>(width_) / static_cast<float>(height_);
		float aspect_ratio_ratio = internal_aspect_ratio / aspect_ratio;

		buffer[0] = -aspect_ratio_ratio;	buffer[1] = -1.0f;
		buffer[4] = -aspect_ratio_ratio;	buffer[5] = 1.0f;
		buffer[8] = aspect_ratio_ratio;		buffer[9] = -1.0f;
		buffer[12] = aspect_ratio_ratio;	buffer[13] = 1.0f;

		// upload buffer
		glBindBuffer(GL_ARRAY_BUFFER, drawing_array_buffer_);
		glBufferData(GL_ARRAY_BUFFER, sizeof(buffer), buffer, GL_STATIC_DRAW);
	}

	pixel_shader_->bind();
	glUniform1f(threshold_uniform_, colour_threshold);

	glBindVertexArray(drawing_vertex_array_);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}
