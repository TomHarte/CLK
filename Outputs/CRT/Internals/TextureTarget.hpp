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

class TextureTarget {
	public:
		TextureTarget(GLsizei width, GLsizei height);
		~TextureTarget();

		void bind_framebuffer();
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
