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
#include <stdexcept>

using namespace Outputs::Display::OpenGL;

TextureTarget::TextureTarget(GLsizei width, GLsizei height, GLenum texture_unit, GLint mag_filter, bool has_stencil_buffer) :
		width_(width),
		height_(height),
		texture_unit_(texture_unit) {
	// Generate and bind a frame buffer.
	test_gl(glGenFramebuffers, 1, &framebuffer_);
	test_gl(glBindFramebuffer, GL_FRAMEBUFFER, framebuffer_);

	// Round the width and height up to the next power of two.
	expanded_width_ = 1;
	while(expanded_width_ < width)		expanded_width_ <<= 1;
	expanded_height_ = 1;
	while(expanded_height_ < height)	expanded_height_ <<= 1;

	// Generate a texture and bind it to the nominated texture unit.
	test_gl(glGenTextures, 1, &texture_);
	bind_texture();

	// Set dimensions and set the user-supplied magnification filter.
	test_gl(glTexImage2D, GL_TEXTURE_2D, 0, GL_RGBA, GLsizei(expanded_width_), GLsizei(expanded_height_), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	test_gl(glTexParameteri, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
	test_gl(glTexParameteri, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	// Set the texture as colour attachment 0 on the frame buffer.
	test_gl(glFramebufferTexture2D, GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_, 0);

	// Also add a stencil buffer if requested.
	if(has_stencil_buffer) {
		test_gl(glGenRenderbuffers, 1, &renderbuffer_);
		test_gl(glBindRenderbuffer, GL_RENDERBUFFER, renderbuffer_);
		test_gl(glRenderbufferStorage, GL_RENDERBUFFER, GL_STENCIL_INDEX8, expanded_width_, expanded_height_);
		test_gl(glFramebufferRenderbuffer, GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, renderbuffer_);
	}

	// Check for successful construction.
	const auto framebuffer_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if(framebuffer_status != GL_FRAMEBUFFER_COMPLETE) {
		switch(framebuffer_status) {
			case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
				throw std::runtime_error("GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT");
			case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
				throw std::runtime_error("GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER");
			case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
				throw std::runtime_error("GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER");
			case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
				throw std::runtime_error("GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT");
			case GL_FRAMEBUFFER_UNSUPPORTED:
				throw std::runtime_error("GL_FRAMEBUFFER_UNSUPPORTED");
			default:
				throw std::runtime_error("Framebuffer status incomplete: " + std::to_string(framebuffer_status));

			case 0:
				test_gl_error();
			break;
		}
	}

	// Clear the framebuffer.
	test_gl(glClear, GL_COLOR_BUFFER_BIT);
}

TextureTarget::~TextureTarget() {
	glDeleteFramebuffers(1, &framebuffer_);
	glDeleteTextures(1, &texture_);
	if(renderbuffer_) glDeleteRenderbuffers(1, &renderbuffer_);
	if(drawing_vertex_array_) glDeleteVertexArrays(1, &drawing_vertex_array_);
	if(drawing_array_buffer_) glDeleteBuffers(1, &drawing_array_buffer_);
}

void TextureTarget::bind_framebuffer() {
	test_gl(glBindFramebuffer, GL_FRAMEBUFFER, framebuffer_);
	test_gl(glViewport, 0, 0, width_, height_);
}

void TextureTarget::bind_texture() const {
	test_gl(glActiveTexture, texture_unit_);
	test_gl(glBindTexture, GL_TEXTURE_2D, texture_);
}

void TextureTarget::draw(float aspect_ratio, float colour_threshold) const {
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
		pixel_shader_ = std::make_unique<Shader>(vertex_shader, fragment_shader);
		pixel_shader_->bind();

		test_gl(glGenVertexArrays, 1, &drawing_vertex_array_);
		test_gl(glGenBuffers, 1, &drawing_array_buffer_);

		test_gl(glBindVertexArray, drawing_vertex_array_);
		test_gl(glBindBuffer, GL_ARRAY_BUFFER, drawing_array_buffer_);

		const GLint position_attribute	= pixel_shader_->get_attrib_location("position");
		const GLint tex_coord_attribute	= pixel_shader_->get_attrib_location("texCoord");

		test_gl(glEnableVertexAttribArray, GLuint(position_attribute));
		test_gl(glEnableVertexAttribArray, GLuint(tex_coord_attribute));

		const GLsizei vertex_stride = 4 * sizeof(GLfloat);
		test_gl(glVertexAttribPointer, GLuint(position_attribute),	2, GL_FLOAT,	GL_FALSE,	vertex_stride,	(void *)0);
		test_gl(glVertexAttribPointer, GLuint(tex_coord_attribute),	2, GL_FLOAT,	GL_FALSE,	vertex_stride,	(void *)(2 * sizeof(GLfloat)));

		const GLint texIDUniform = pixel_shader_->get_uniform_location("texID");
		test_gl(glUniform1i, texIDUniform, GLint(texture_unit_ - GL_TEXTURE0));

		threshold_uniform_ = pixel_shader_->get_uniform_location("threshold");
	}

	if(set_aspect_ratio_ != aspect_ratio) {
		set_aspect_ratio_ = aspect_ratio;
		float buffer[4*4];

		// establish texture coordinates
		buffer[2] = 0.0f;
		buffer[3] = 0.0f;
		buffer[6] = 0.0f;
		buffer[7] = float(height_) / float(expanded_height_);
		buffer[10] = float(width_) / float(expanded_width_);
		buffer[11] = 0.0f;
		buffer[14] = buffer[10];
		buffer[15] = buffer[7];

		// determine positions; rule is to keep the same height and centre
		float internal_aspect_ratio = float(width_) / float(height_);
		float aspect_ratio_ratio = internal_aspect_ratio / aspect_ratio;

		buffer[0] = -aspect_ratio_ratio;	buffer[1] = -1.0f;
		buffer[4] = -aspect_ratio_ratio;	buffer[5] = 1.0f;
		buffer[8] = aspect_ratio_ratio;		buffer[9] = -1.0f;
		buffer[12] = aspect_ratio_ratio;	buffer[13] = 1.0f;

		// upload buffer
		test_gl(glBindBuffer, GL_ARRAY_BUFFER, drawing_array_buffer_);
		test_gl(glBufferData, GL_ARRAY_BUFFER, sizeof(buffer), buffer, GL_STATIC_DRAW);
	}

	pixel_shader_->bind();
	test_gl(glUniform1f, threshold_uniform_, colour_threshold);

	test_gl(glBindVertexArray, drawing_vertex_array_);
	test_gl(glDrawArrays, GL_TRIANGLE_STRIP, 0, 4);
}
