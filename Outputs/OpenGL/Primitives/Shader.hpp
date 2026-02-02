//
//  Shader.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/02/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#pragma once

#include "Outputs/OpenGL/API.hpp"
#include "Outputs/OpenGL/OpenGL.hpp"

#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace Outputs::Display::OpenGL {

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
		AttributeBinding(const std::string &name, const GLuint index) : name(name), index(index) {}
		const std::string name;
		const GLuint index;
	};

	/*!
		Attempts to compile a shader, throwing @c VertexShaderCompilationError, @c FragmentShaderCompilationError
		or @c ProgramLinkageError upon failure.
		@param vertex_shader The vertex shader source code.
		@param fragment_shader The fragment shader source code.
		@param attribute_bindings A vector of attribute bindings.
	*/
	Shader(
		API,
		const std::string &vertex_shader,
		const std::string &fragment_shader,
		const std::vector<AttributeBinding> & = {}
	);
	/*!
		Attempts to compile a shader, throwing @c VertexShaderCompilationError, @c FragmentShaderCompilationError
		or @c ProgramLinkageError upon failure.
		@param vertex_shader The vertex shader source code.
		@param fragment_shader The fragment shader source code.
		@param binding_names A list of attributes to generate bindings for; these will be given indices 0, 1, 2 ... n-1.
	*/
	Shader(
		API,
		const std::string &vertex_shader,
		const std::string &fragment_shader,
		const std::vector<std::string> &binding_names
	);
	~Shader();

	// Allow moves, including move assignment, and default construction to create something vacant.
	Shader(Shader&&);
	Shader &operator =(Shader &&);
	Shader() = default;

	/*!
		Performs an @c glUseProgram to make this the active shader unless:
			(i) it was the previous shader bound; and
			(ii) no calls have been received to unbind in the interim.

		Subsequently performs all work queued up for the next bind irrespective of whether a @c glUseProgram call occurred.
	*/
	void bind() const;

	/*!
		Unbinds the current instance of Shader, if one is bound.
	*/
	static void unbind();

	/*!
		Performs a @c glBindAttribLocation call.
		@param name The name of the attribute to bind.
		@param index The index to bind to.
	*/
	void bind_attrib_location(const std::string &name, GLuint index);

	/*!
		Performs a @c glGetAttribLocation call.
		@param name The name of the attribute to locate.
		@returns The location of the requested attribute.
	*/
	GLint get_attrib_location(const std::string &name) const;

	/*!
		Performs a @c glGetUniformLocation call.
		@param name The name of the uniform to locate.
		@returns The location of the requested uniform.
	*/
	GLint get_uniform_location(const std::string &name) const;

	/*!
		Shorthand for an appropriate sequence of:
		* @c get_attrib_location;
		* @c glEnableVertexAttribArray;
		* @c glVertexAttribPointer;
		* @c glVertexAttribDivisor.
	*/
	void enable_vertex_attribute_with_pointer(
		const std::string &name,
		GLint size,
		GLenum type,
		GLboolean normalised,
		GLsizei stride,
		const GLvoid *pointer,
		GLuint divisor
	);

	/*!
		All @c set_uniforms queue up the requested uniform changes. Changes are applied automatically the next time the shader is bound.
	*/
	void set_uniform(const std::string &name, GLint);
	void set_uniform(const std::string &name, GLint, GLint);
	void set_uniform(const std::string &name, GLint, GLint, GLint);
	void set_uniform(const std::string &name, GLint, GLint, GLint, GLint);
	void set_uniform(const std::string &name, GLint size, GLsizei count, const GLint *);

	void set_uniform(const std::string &name, GLfloat);
	void set_uniform(const std::string &name, GLfloat, GLfloat value2);
	void set_uniform(const std::string &name, GLfloat, GLfloat, GLfloat);
	void set_uniform(const std::string &name, GLfloat, GLfloat, GLfloat, GLfloat);
	void set_uniform(const std::string &name, GLint size, GLsizei count, const GLfloat *);

	void set_uniform(const std::string &name, GLuint);
	void set_uniform(const std::string &name, GLuint, GLuint);
	void set_uniform(const std::string &name, GLuint, GLuint, GLuint);
	void set_uniform(const std::string &name, GLuint, GLuint, GLuint, GLuint);
	void set_uniform(const std::string &name, GLint size, GLsizei count, const GLuint *);

	void set_uniform_matrix(const std::string &name, GLint size, bool transpose, const GLfloat *values);
	void set_uniform_matrix(const std::string &name, GLint size, GLsizei count, bool transpose, const GLfloat *values);

	bool empty() const {
		return shader_program_ == 0;
	}

private:
	void init(
		const std::string &vertex_shader,
		const std::string &fragment_shader,
		const std::vector<AttributeBinding> &
	);

	GLuint compile_shader(const std::string &source, GLenum type);
	API api_;
	GLuint shader_program_ = 0;
};

}
