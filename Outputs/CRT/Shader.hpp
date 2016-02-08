//
//  Shader.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/02/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Shader_hpp
#define Shader_hpp

#include "OpenGL.hpp"

namespace OpenGL {
  class Shader {
	public:
		Shader(const char *vertex_shader, const char *fragment_shader);
		~Shader();

		void bind();
		GLint get_attrib_location(const char *name);
		GLint get_uniform_location(const char *name);

	private:
		GLuint compile_shader(const char *source, GLenum type);
		GLuint _shader_program;
  };
}

#endif /* Shader_hpp */
