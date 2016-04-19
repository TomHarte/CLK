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
		struct AttributeBinding {
			const GLchar *name;
			GLuint index;
		};

		/*!
			Constructs a shader, comprised of:
			@param vertex_shader The vertex shader source code.
			@param fragment_shader The fragment shader source code.
			@param attribute_bindings Either @c nullptr or an array terminated by an entry with a @c nullptr-name of attribute bindings.
		*/
		Shader(const char *vertex_shader, const char *fragment_shader, const AttributeBinding *attribute_bindings);
		~Shader();

		/*!
			Performs an @c glUseProgram to make this the active shader.
		*/
		void bind();

		/*!
			Performs a @c glGetAttribLocation call.
			@param name The name of the attribute to locate.
			@returns The location of the requested attribute.
		*/
		GLint get_attrib_location(const GLchar *name);

		/*!
			Performs a @c glGetUniformLocation call.
			@param name The name of the uniform to locate.
			@returns The location of the requested uniform.
		*/
		GLint get_uniform_location(const GLchar *name);

	private:
		GLuint compile_shader(const char *source, GLenum type);
		GLuint _shader_program;
  };
}

#endif /* Shader_hpp */
