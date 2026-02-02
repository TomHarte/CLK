//
//  Shader.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/02/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "Shader.hpp"

#include "Outputs/Log.hpp"
#include <vector>

using namespace Outputs::Display::OpenGL;

namespace {
thread_local const Shader *bound_shader = nullptr;
using Logger = Log::Logger<Log::Source::OpenGL>;
}

GLuint Shader::compile_shader(const std::string &source, const GLenum type) {
	const GLuint shader = glCreateShader(type);

	switch(api_) {
		case API::OpenGL32Core: {
			// Desktop OpenGL: ensure the precision specifiers act as no-ops
			// and request GLSL 1.5.
			const char *const sources[] = {
				R"glsl(
					#version 150
					#define highp
					#define mediump
					#define lowp
				)glsl",
				source.c_str()
			};
			test_gl([&]{ glShaderSource(shader, 2, sources, NULL); });
		} break;
		case API::OpenGLES3:
			// OpenGL ES: supply default precisions for where they might have
			// been omitted and specify GLSL ES 3.0 as a floor. The project
			// otherwise assumes that integers and bitwise operations are available.
			const char *const sources[] = {
				R"glsl(
					#version 300 es
					precision highp float;
					precision highp usampler2D;
				)glsl",
				source.c_str()
			};
			test_gl([&]{ glShaderSource(shader, 2, sources, NULL); });
		break;
	}
	test_gl([&]{ glCompileShader(shader); });

	GLint is_compiled = 0;
	test_gl([&]{ glGetShaderiv(shader, GL_COMPILE_STATUS, &is_compiled); });
	if(is_compiled == GL_FALSE) {
		if constexpr (Logger::ErrorsEnabled) {
			Logger::error().append("Failed to compile: %s", source.c_str());
			GLint log_length;
			test_gl([&]{ glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length); });
			if(log_length > 0) {
				const auto length = std::vector<GLchar>::size_type(log_length);
				std::vector<GLchar> log(length);
				test_gl([&]{ glGetShaderInfoLog(shader, log_length, &log_length, log.data()); });
				Logger::error().append("Compile log: %s", log.data());
			}
		}

		throw (type == GL_VERTEX_SHADER) ? VertexShaderCompilationError : FragmentShaderCompilationError;
	}

	return shader;
}

Shader::Shader(
	const API api,
	const std::string &vertex_shader,
	const std::string &fragment_shader,
	const std::vector<AttributeBinding> &attribute_bindings
) : api_(api) {
	init(vertex_shader, fragment_shader, attribute_bindings);
}

Shader::Shader(
	const API api,
	const std::string &vertex_shader,
	const std::string &fragment_shader,
	const std::vector<std::string> &binding_names
) : api_(api) {
	std::vector<AttributeBinding> bindings;
	GLuint index = 0;
	for(const auto &name: binding_names) {
		bindings.emplace_back(name, index);
		++index;
	}
	init(vertex_shader, fragment_shader, bindings);
}

void Shader::init(
	const std::string &vertex_shader,
	const std::string &fragment_shader,
	const std::vector<AttributeBinding> &attribute_bindings
) {
	shader_program_ = glCreateProgram();
	const GLuint vertex = compile_shader(vertex_shader, GL_VERTEX_SHADER);
	const GLuint fragment = compile_shader(fragment_shader, GL_FRAGMENT_SHADER);

	test_gl([&]{ glAttachShader(shader_program_, vertex); });
	test_gl([&]{ glAttachShader(shader_program_, fragment); });

	for(const auto &binding : attribute_bindings) {
		bind_attrib_location(binding.name, binding.index);
	}

	test_gl([&]{ glLinkProgram(shader_program_); });

	GLint did_link = 0;
	test_gl([&]{ glGetProgramiv(shader_program_, GL_LINK_STATUS, &did_link); });
	if(did_link == GL_FALSE) {
		if constexpr (Logger::ErrorsEnabled) {
			GLint log_length;
			test_gl([&]{ glGetProgramiv(shader_program_, GL_INFO_LOG_LENGTH, &log_length); });
			if(log_length > 0) {
				std::vector<GLchar> log(log_length);
				test_gl([&]{ glGetProgramInfoLog(shader_program_, log_length, &log_length, log.data()); });
				Logger::error().append("Link log: %s", log.data());
			}
		}

		throw ProgramLinkageError;
	}
}

Shader &Shader::operator =(Shader &&rhs) {
	api_ = rhs.api_;
	std::swap(shader_program_, rhs.shader_program_);

	if(bound_shader == &rhs) {
		bound_shader = this;
	}
	return *this;
}

Shader::Shader(Shader &&rhs) {
	*this = std::move(rhs);
}

Shader::~Shader() {
	if(bound_shader == this) Shader::unbind();
	glDeleteProgram(shader_program_);
}

void Shader::bind() const {
	if(bound_shader != this) {
		test_gl([&]{ glUseProgram(shader_program_); });
		bound_shader = this;
	}
}

void Shader::unbind() {
	bound_shader = nullptr;
	test_gl([&]{ glUseProgram(0); });
}

void Shader::bind_attrib_location(const std::string &name, const GLuint index) {
	test_gl([&]{ glBindAttribLocation(shader_program_, index, name.c_str()); });

	if constexpr (Logger::ErrorsEnabled) {
		const auto error = glGetError();
		switch(error) {
			case 0: break;
			case GL_INVALID_VALUE:
				Logger::error().append(
					"GL_INVALID_VALUE when attempting to bind %s to index %d "
					"(i.e. index is greater than or equal to GL_MAX_VERTEX_ATTRIBS)",
						name.c_str(), index);
			break;
			case GL_INVALID_OPERATION:
				Logger::error().append(
					"GL_INVALID_OPERATION when attempting to bind %s to index %d "
					"(i.e. name begins with gl_)",
						name.c_str(), index);
			break;
			default:
				Logger::error().append(
					"Error %d when attempting to bind %s to index %d", error, name.c_str(), index);
			break;
		}
	}
}

GLint Shader::get_attrib_location(const std::string &name) const {
	return glGetAttribLocation(shader_program_, name.c_str());
}

GLint Shader::get_uniform_location(const std::string &name) const {
	const auto location = glGetUniformLocation(shader_program_, name.c_str());
	test_gl_error();
	return location;
}

void Shader::enable_vertex_attribute_with_pointer(
	const std::string &name,
	const GLint size,
	const GLenum type,
	const GLboolean normalised,
	const GLsizei stride,
	const GLvoid *const pointer,
	const GLuint divisor
) {
	const auto location = get_attrib_location(name);
	if(location >= 0) {
		test_gl([&]{ glEnableVertexAttribArray(GLuint(location)); });
		test_gl([&]{ glVertexAttribPointer(GLuint(location), size, type, normalised, stride, pointer); });
		test_gl([&]{ glVertexAttribDivisor(GLuint(location), divisor); });
	} else {
		Logger::error().append("Couldn't enable vertex attribute %s", name.c_str());
	}
}

// The various set_uniforms...
#define with_location(func, ...) {\
		bind();	\
		const GLint location = glGetUniformLocation(shader_program_, name.c_str());	\
		if(location == -1) { \
			Logger::error().append("Couldn't get location for uniform %s", name.c_str());	\
		} else { \
			func(location, __VA_ARGS__);	\
			if(glGetError()) Logger::error().append("Error setting uniform %s via %s", name.c_str(), #func);	\
		} \
	}

void Shader::set_uniform(const std::string &name, const GLint value) {
	with_location(glUniform1i, value);
}

void Shader::set_uniform(const std::string &name, const GLuint value) {
	with_location(glUniform1ui, value);
}

void Shader::set_uniform(const std::string &name, const GLfloat value) {
	with_location(glUniform1f, value);
}


void Shader::set_uniform(const std::string &name, const GLint value1, const GLint value2) {
	with_location(glUniform2i, value1, value2);
}

void Shader::set_uniform(const std::string &name, const GLfloat value1, const GLfloat value2) {
	with_location(glUniform2f, value1, value2);
}

void Shader::set_uniform(const std::string &name, const GLuint value1, const GLuint value2) {
	with_location(glUniform2ui, value1, value2);
}

void Shader::set_uniform(const std::string &name, const GLint value1, const GLint value2, const GLint value3) {
	with_location(glUniform3i, value1, value2, value3);
}

void Shader::set_uniform(const std::string &name, const GLfloat value1, const GLfloat value2, const GLfloat value3) {
	with_location(glUniform3f, value1, value2, value3);
}

void Shader::set_uniform(const std::string &name, const GLuint value1, const GLuint value2, const GLuint value3) {
	with_location(glUniform3ui, value1, value2, value3);
}

void Shader::set_uniform(
	const std::string &name,
	const GLint value1,
	const GLint value2,
	const GLint value3,
	const GLint value4
) {
	with_location(glUniform4i, value1, value2, value3, value4);
}

void Shader::set_uniform(
	const std::string &name,
	const GLfloat value1,
	const GLfloat value2,
	const GLfloat value3,
	const GLfloat value4
) {
	with_location(glUniform4f, value1, value2, value3, value4);
}

void Shader::set_uniform(
	const std::string &name,
	const GLuint value1,
	const GLuint value2,
	const GLuint value3,
	const GLuint value4
) {
	with_location(glUniform4ui, value1, value2, value3, value4);
}

void Shader::set_uniform(
	const std::string &name,
	const GLint size,
	const GLsizei count,
	const GLint *const values
) {
	switch(size) {
		case 1: with_location(glUniform1iv, count, values);	break;
		case 2: with_location(glUniform2iv, count, values);	break;
		case 3: with_location(glUniform3iv, count, values);	break;
		case 4: with_location(glUniform4iv, count, values);	break;
	}
}

void Shader::set_uniform(const std::string &name, const GLint size, const GLsizei count, const GLfloat *const values) {
	switch(size) {
		case 1: with_location(glUniform1fv, count, values);	break;
		case 2: with_location(glUniform2fv, count, values);	break;
		case 3: with_location(glUniform3fv, count, values);	break;
		case 4: with_location(glUniform4fv, count, values);	break;
	}
}

void Shader::set_uniform(const std::string &name, const GLint size, const GLsizei count, const GLuint *const values) {
	switch(size) {
		case 1: with_location(glUniform1uiv, count, values);	break;
		case 2: with_location(glUniform2uiv, count, values);	break;
		case 3: with_location(glUniform3uiv, count, values);	break;
		case 4: with_location(glUniform4uiv, count, values);	break;
	}
}

void Shader::set_uniform_matrix(
	const std::string &name,
	const GLint size,
	const bool transpose,
	const GLfloat *const values
) {
	set_uniform_matrix(name, size, 1, transpose, values);
}

void Shader::set_uniform_matrix(
	const std::string &name,
	const GLint size,
	const GLsizei count,
	const bool transpose,
	const GLfloat *const values
) {
	const GLboolean glTranspose = transpose ? GL_TRUE : GL_FALSE;
	switch(size) {
		case 2: with_location(glUniformMatrix2fv, count, glTranspose, values);	break;
		case 3: with_location(glUniformMatrix3fv, count, glTranspose, values);	break;
		case 4: with_location(glUniformMatrix4fv, count, glTranspose, values);	break;
	}
}
