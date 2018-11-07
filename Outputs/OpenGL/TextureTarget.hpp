//
//  TextureTarget.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/02/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
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
			return width_;
		}

		/*!
			@returns the height of the texture target.
		*/
		GLsizei get_height() {
			return height_;
		}

		/*!
			Draws this texture to the currently-bound framebuffer, which has the aspect ratio
			@c aspect_ratio. This texture will fill the height of the frame buffer, and pick
			an appropriate width based on the aspect ratio.

			@c colour_threshold sets a threshold test that each colour must satisfy to be
			output. A threshold of 0.0f means that all colours will pass through. A threshold
			of 0.5f means that only colour components above 0.5f will pass through, with
			0.5f being substituted elsewhere. This provides a way to ensure that the sort of
			persistent low-value errors that can result from an IIR are hidden.
		*/
		void draw(float aspect_ratio, float colour_threshold = 0.0f);

		enum {
			ErrorFramebufferIncomplete
		};

	private:
		GLuint framebuffer_ = 0, texture_ = 0;
		GLsizei width_ = 0, height_ = 0;
		GLsizei expanded_width_ = 0, expanded_height_ = 0;
		GLenum texture_unit_ = 0;

		std::unique_ptr<Shader> pixel_shader_;
		GLuint drawing_vertex_array_ = 0, drawing_array_buffer_ = 0;
		float set_aspect_ratio_ = 0.0f;

		GLint threshold_uniform_;
};

}

#endif /* TextureTarget_hpp */
