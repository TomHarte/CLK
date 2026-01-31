//
//  Texture.hpp
//  Clock Signal Kiosk
//
//  Created by Thomas Harte on 30/01/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include "Outputs/OpenGL/OpenGL.hpp"

namespace Outputs::Display::OpenGL {

/*!
	Holds a texture of size @c width and @c height, which is bound to @c texture_unit.

	Textures are always a single byte per channel. Both clamp directions are set to `GL_CLAMP_TO_EDGE`.
	The magnification and minification filters are as specified.
*/
class Texture {
public:
	Texture(
		size_t channels,
		GLenum texture_unit,
		GLint mag_filter,
		GLint min_filter,
		GLsizei width,
		GLsizei height
	);
	~Texture();

	Texture() = default;
	Texture(Texture &&);
	Texture &operator =(Texture &&);

	GLsizei width() const	{	return width_;	}
	GLsizei height() const	{	return height_;	}

	/*!
		Binds this texture; sets the active texture unit as a side effect.
	*/
	void bind();

	bool empty() const {
		return texture_ == 0;
	}

private:
	GLenum texture_unit_ = GL_TEXTURE0;
	GLuint texture_ = 0;
	GLsizei width_ = 0, height_ = 0;
};

}
