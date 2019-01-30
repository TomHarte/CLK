//
//  Shader.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/02/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#ifndef Shader_hpp
#define Shader_hpp

#include "../OpenGL.hpp"

#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace Outputs {
namespace Display {
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
		AttributeBinding(const std::string &name, GLuint index) : name(name), index(index) {}
		const std::string name;
		const GLuint index;
	};

	/*!
		Attempts to compile a shader, throwing @c VertexShaderCompilationError, @c FragmentShaderCompilationError or @c ProgramLinkageError upon failure.
		@param vertex_shader The vertex shader source code.
		@param fragment_shader The fragment shader source code.
		@param attribute_bindings A vector of attribute bindings.
	*/
	Shader(const std::string &vertex_shader, const std::string &fragment_shader, const std::vector<AttributeBinding> &attribute_bindings = {});
	/*!
		Attempts to compile a shader, throwing @c VertexShaderCompilationError, @c FragmentShaderCompilationError or @c ProgramLinkageError upon failure.
		@param vertex_shader The vertex shader source code.
		@param fragment_shader The fragment shader source code.
		@param binding_names A list of attributes to generate bindings for; these will be given indices 0, 4, 8 ... 4(n-1).
	*/
	Shader(const std::string &vertex_shader, const std::string &fragment_shader, const std::vector<std::string> &binding_names);
	~Shader();

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
	void enable_vertex_attribute_with_pointer(const std::string &name, GLint size, GLenum type, GLboolean normalised, GLsizei stride, const GLvoid *pointer, GLuint divisor);

	/*!
		All @c set_uniforms queue up the requested uniform changes. Changes are applied automatically the next time the shader is bound.
	*/
	void set_uniform(const std::string &name, GLint value);
	void set_uniform(const std::string &name, GLint value1, GLint value2);
	void set_uniform(const std::string &name, GLint value1, GLint value2, GLint value3);
	void set_uniform(const std::string &name, GLint value1, GLint value2, GLint value3, GLint value4);
	void set_uniform(const std::string &name, GLint size, GLsizei count, const GLint *values);

	void set_uniform(const std::string &name, GLfloat value);
	void set_uniform(const std::string &name, GLfloat value1, GLfloat value2);
	void set_uniform(const std::string &name, GLfloat value1, GLfloat value2, GLfloat value3);
	void set_uniform(const std::string &name, GLfloat value1, GLfloat value2, GLfloat value3, GLfloat value4);
	void set_uniform(const std::string &name, GLint size, GLsizei count, const GLfloat *values);

	void set_uniform(const std::string &name, GLuint value);
	void set_uniform(const std::string &name, GLuint value1, GLuint value2);
	void set_uniform(const std::string &name, GLuint value1, GLuint value2, GLuint value3);
	void set_uniform(const std::string &name, GLuint value1, GLuint value2, GLuint value3, GLuint value4);
	void set_uniform(const std::string &name, GLint size, GLsizei count, const GLuint *values);

	void set_uniform_matrix(const std::string &name, GLint size, bool transpose, const GLfloat *values);
	void set_uniform_matrix(const std::string &name, GLint size, GLsizei count, bool transpose, const GLfloat *values);

private:
	void init(const std::string &vertex_shader, const std::string &fragment_shader, const std::vector<AttributeBinding> &attribute_bindings);

	GLuint compile_shader(const std::string &source, GLenum type);
	GLuint shader_program_;

	void flush_functions() const;
	mutable std::vector<std::function<void(void)>> enqueued_functions_;
	mutable std::mutex function_mutex_;

protected:
	void enqueue_function(std::function<void(void)> function);
};

}
}
}

#endif /* Shader_hpp */
