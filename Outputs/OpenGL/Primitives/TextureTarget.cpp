//
//  TextureTarget.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/02/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "TextureTarget.hpp"

#include <algorithm>
#include <stdexcept>

using namespace Outputs::Display::OpenGL;

TextureTarget::TextureTarget(
	const API api,
	const GLsizei width,
	const GLsizei height,
	const GLenum texture_unit,
	const GLint mag_filter,
	const bool has_stencil_buffer
) :
	api_(api),
	width_(width),
	height_(height),
	texture_unit_(texture_unit)
{
	// Generate and bind a frame buffer.
	test_gl([&]{ glGenFramebuffers(1, &framebuffer_); });
	test_gl([&]{ glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_); });

	// Generate a texture and bind it to the nominated texture unit.
	test_gl([&]{ glGenTextures(1, &texture_); });
	bind_texture();

	// Set dimensions and set the user-supplied magnification filter.
	// If this is a debug build, apply a random initial fill.
#ifndef NDEBUG
	std::vector<uint8_t> image(width_ * height_ * 4);
	for(auto &c : image) {
		c = rand();
	}
	const void *source = image.data();
#else
	constexpr void *source = nullptr;
#endif
	test_gl([&]{
		glTexImage2D(
			GL_TEXTURE_2D,
			0,
			GL_RGBA,
			GLsizei(width_),
			GLsizei(height_),
			0,
			GL_RGBA,
			GL_UNSIGNED_BYTE,
			source
		);
	});
	test_gl([&]{ glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter); });
	test_gl([&]{ glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); });

	// Set the texture as colour attachment 0 on the frame buffer.
	test_gl([&]{ glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_, 0); });

	// Also add a stencil buffer if requested.
	if(has_stencil_buffer) {
		test_gl([&]{ glGenRenderbuffers(1, &renderbuffer_); });
		test_gl([&]{ glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer_); });
		test_gl([&]{ glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, width_, height_); });
		test_gl([&]{
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, renderbuffer_);
		});
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
	test_gl([&]{ glClear(GL_COLOR_BUFFER_BIT | (has_stencil_buffer ? GL_STENCIL_BUFFER_BIT : 0)); });
}

TextureTarget::~TextureTarget() {
	glDeleteFramebuffers(1, &framebuffer_);
	glDeleteTextures(1, &texture_);
	glDeleteRenderbuffers(1, &renderbuffer_);
}

TextureTarget::TextureTarget(TextureTarget &&rhs) {
	*this = std::move(rhs);
}

TextureTarget &TextureTarget::operator =(TextureTarget &&rhs) {
	api_ = rhs.api_;
	std::swap(framebuffer_, rhs.framebuffer_);
	std::swap(texture_, rhs.texture_);
	std::swap(renderbuffer_, rhs.renderbuffer_);
	std::swap(width_, rhs.width_);
	std::swap(height_, rhs.height_);
	std::swap(texture_unit_, rhs.texture_unit_);

	// Other fields in the TextureTarget relate to the `draw` functionality below, which I intend to
	// eliminate. Therefore just let them be.

	return *this;
}

void TextureTarget::bind_framebuffer() {
	test_gl([&]{ glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_); });
	test_gl([&]{ glViewport(0, 0, width_, height_); });
}

void TextureTarget::bind_texture() const {
	test_gl([&]{ glActiveTexture(texture_unit_); });
	test_gl([&]{ glBindTexture(GL_TEXTURE_2D, texture_); });
}
