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

namespace OpenGL {

/*!
	A @c TextureTarget is a framebuffer that can be bound as a texture. So this class
	handles render-to-texture framebuffer objects.
*/
class TextureTarget {
	public:
		/*!
			Creates a new texture target.

			Throws ErrorFramebufferIncomplete if creation fails.

			@param width The width of target to create.
			@param height The height of target to create.
		*/
		TextureTarget(GLsizei width, GLsizei height);
		~TextureTarget();

		/*!
			Binds this target as a framebuffer and sets the @c glViewport accordingly.
		*/
		void bind_framebuffer();

		/*!
			Binds this target as a texture.
		*/
		void bind_texture();

		enum {
			ErrorFramebufferIncomplete
		};

	private:
		GLuint _framebuffer, _texture;
		GLsizei _width, _height;
};

}

#endif /* TextureTarget_hpp */
