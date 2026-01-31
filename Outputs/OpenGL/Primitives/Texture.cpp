//
//  Texture.cpp
//  Clock Signal Kiosk
//
//  Created by Thomas Harte on 30/01/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include "Texture.hpp"

using namespace Outputs::Display::OpenGL;

Texture::Texture(
	const GLenum texture_unit,
	const GLint mag_filter,
	const GLint min_filter,
	const GLsizei width,
	const GLsizei height
) :
	texture_unit_(texture_unit),
	width_(width),
	height_(height)
{
	test_gl(glGenTextures, 1, &texture_);
	test_gl(glActiveTexture, texture_unit);
	test_gl(glBindTexture, GL_TEXTURE_2D, texture_);

	test_gl(
		glTexImage2D,
		GL_TEXTURE_2D,
		0,
		GL_RGBA,
		GLsizei(width_),
		GLsizei(height_),
		0,
		GL_RGBA,
		GL_UNSIGNED_BYTE,
		nullptr
	);
	test_gl(glTexParameteri, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
	test_gl(glTexParameteri, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
	test_gl(glTexParameteri, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	test_gl(glTexParameteri, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void Texture::bind() {
	test_gl(glActiveTexture, texture_unit_);
	test_gl(glBindTexture, GL_TEXTURE_2D, texture_);
}

Texture::~Texture() {
	glDeleteTextures(1, &texture_);
}

Texture::Texture(Texture &&rhs) {
	*this = std::move(rhs);
}

Texture &Texture::operator =(Texture &&rhs) {
	std::swap(texture_, rhs.texture_);
	std::swap(texture_unit_, rhs.texture_unit_);
	std::swap(width_, rhs.width_);
	std::swap(height_, rhs.height_);
	return *this;
}
