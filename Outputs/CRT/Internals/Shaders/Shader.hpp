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

/*!
	A @c Shader compiles and holds a shader object, based on a single
	vertex program and a single fragment program. Attribute bindings
	may be supplied if desired.
*/
class Shader {
public:
	enum {
		VertexShaderCompilationError,
		FragmentShaderCompilationError,
		ProgramLinkageError
	};

	struct AttributeBinding {
		const GLchar *name;
		GLuint index;
	};

	/*!
		Attempts to compile a shader, throwing @c VertexShaderCompilationError, @c FragmentShaderCompilationError or @c ProgramLinkageError upon failure.
		@param vertex_shader The vertex shader source code.
		@param fragment_shader The fragment shader source code.
		@param attribute_bindings Either @c nullptr or an array terminated by an entry with a @c nullptr-name of attribute bindings.
	*/
	Shader(const char *vertex_shader, const char *fragment_shader, const AttributeBinding *attribute_bindings);
	~Shader();

	/*!
		Performs an @c glUseProgram to make this the active shader unless:
			(i) it was the previous shader bound; and 
			(ii) no calls have been received to unbind in the interim.
	*/
	void bind();

	/*!
		Unbinds the current instance of Shader, if one is bound.
	*/
	static void unbind();

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

	/*!
		Shorthand for an appropriate sequence of:
		* @c get_attrib_location;
		* @c glEnableVertexAttribArray;
		* @c glVertexAttribPointer;
		* @c glVertexAttribDivisor.
	*/
	void enable_vertex_attribute_with_pointer(const char *name, GLint size, GLenum type, GLboolean normalised, GLsizei stride, const GLvoid *pointer, GLuint divisor);

private:
	GLuint compile_shader(const char *source, GLenum type);
	GLuint _shader_program;
};

}

#endif /* Shader_hpp */
