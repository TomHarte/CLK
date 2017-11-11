//
//  TextureTarget.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/02/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef TextureTarget_hpp
#define TextureTarget_hpp

#include "OpenGL.hpp"
#include "Shaders/Shader.hpp"
#include <memory>

namespace OpenGL {

/*!
	A @c TextureTarget is a framebuffer that can be bound as a texture. So this class
	handles render-to-texture framebuffer objects.
*/
class TextureTarget {
	public:
		/*!
			Creates a new texture target.

			Throws ErrorFramebufferIncomplete if creation fails. Leaves both the generated texture and framebuffer bound.

			@param width The width of target to create.
			@param height The height of target to create.
			@param texture_unit A texture unit on which to bind the texture.
		*/
		TextureTarget(GLsizei width, GLsizei height, GLenum texture_unit, GLint mag_filter);
		~TextureTarget();

		/*!
			Binds this target as a framebuffer and sets the @c glViewport accordingly.
		*/
		void bind_framebuffer();

		/*!
			Binds this target as a texture.
		*/
		void bind_texture();

		/*!
			@returns the width of the texture target.
		*/
		GLsizei get_width() {
			return _width;
		}

		/*!
			@returns the height of the texture target.
		*/
		GLsizei get_height() {
			return _height;
		}

		/*!

		*/
		void draw(float aspect_ratio);

		enum {
			ErrorFramebufferIncomplete
		};

	private:
		GLuint _framebuffer, _texture;
		GLsizei _width, _height;
		GLsizei _expanded_width, _expanded_height;
		GLenum _texture_unit;

		std::unique_ptr<Shader> _pixel_shader;
		GLuint _drawing_vertex_array = 0, _drawing_array_buffer = 0;
		float _set_aspect_ratio;
};

}

#endif /* TextureTarget_hpp */
