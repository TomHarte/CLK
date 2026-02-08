//
//  TextureTarget.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/02/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#pragma once

#include "Outputs/OpenGL/API.hpp"
#include "Outputs/OpenGL/OpenGL.hpp"
#include "Shader.hpp"

#include <memory>

namespace Outputs::Display::OpenGL {

/*!
	A @c TextureTarget is a framebuffer that can be bound as a texture. So this class
	handles render-to-texture framebuffer objects.
*/
class TextureTarget {
public:
	/*!
		Creates a new texture target. Contents are initially undefined.

		Leaves both the generated texture and framebuffer bound.

		@throws std::runtime_error if creation fails.

		@param width The width of target to create.
		@param height The height of target to create.
		@param texture_unit A texture unit on which to bind the texture.
		@param has_stencil_buffer An 8-bit stencil buffer is attached if this is @c true; no stencil buffer is attached otherwise.
	*/
	TextureTarget(API, GLsizei width, GLsizei height, GLenum texture_unit, GLint mag_filter, bool has_stencil_buffer);
	~TextureTarget();

	TextureTarget() = default;
	TextureTarget(TextureTarget &&);
	TextureTarget &operator =(TextureTarget &&);

	TextureTarget(const TextureTarget &) = delete;
	TextureTarget &operator =(const TextureTarget &) = delete;

	/*!
		Binds this target as a framebuffer and sets the @c glViewport accordingly.
	*/
	void bind_framebuffer();

	/*!
		Binds this target as a texture.
	*/
	void bind_texture() const;

	/*!
		@returns the width of the texture target.
	*/
	GLsizei width() const {
		return width_;
	}

	/*!
		@returns the height of the texture target.
	*/
	GLsizei height() const {
		return height_;
	}

	bool empty() const {
		return framebuffer_ == 0;
	}

	void reset() {
		*this = TextureTarget();
	}

private:
	API api_{};
	GLuint framebuffer_ = 0, texture_ = 0, renderbuffer_ = 0;
	GLsizei width_ = 0, height_ = 0;
	GLenum texture_unit_ = 0;
};

}
