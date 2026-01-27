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
			test_gl(glShaderSource, shader, 2, sources, NULL);
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
			test_gl(glShaderSource, shader, 2, sources, NULL);
		break;
	}
	test_gl(glCompileShader, shader);

	if constexpr (Logger::ErrorsEnabled) {
		GLint is_compiled = 0;
		test_gl(glGetShaderiv, shader, GL_COMPILE_STATUS, &is_compiled);
		if(is_compiled == GL_FALSE) {
			Logger::error().append("Failed to compile: %s", source.c_str());
			GLint log_length;
			test_gl(glGetShaderiv, shader, GL_INFO_LOG_LENGTH, &log_length);
			if(log_length > 0) {
				const auto length = std::vector<GLchar>::size_type(log_length);
				std::vector<GLchar> log(length);
				test_gl(glGetShaderInfoLog, shader, log_length, &log_length, log.data());
				Logger::error().append("Compile log: %s", log.data());
			}

			throw (type == GL_VERTEX_SHADER) ? VertexShaderCompilationError : FragmentShaderCompilationError;
		}
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

	test_gl(glAttachShader, shader_program_, vertex);
	test_gl(glAttachShader, shader_program_, fragment);

	for(const auto &binding : attribute_bindings) {
		test_gl(glBindAttribLocation, shader_program_, binding.index, binding.name.c_str());

		if constexpr (Logger::ErrorsEnabled) {
			const auto error = glGetError();
			switch(error) {
				case 0: break;
				case GL_INVALID_VALUE:
					Logger::error().append(
						"GL_INVALID_VALUE when attempting to bind %s to index %d "
						"(i.e. index is greater than or equal to GL_MAX_VERTEX_ATTRIBS)",
							binding.name.c_str(), binding.index);
				break;
				case GL_INVALID_OPERATION:
					Logger::error().append(
						"GL_INVALID_OPERATION when attempting to bind %s to index %d "
						"(i.e. name begins with gl_)",
							binding.name.c_str(), binding.index);
				break;
				default:
					Logger::error().append(
						"Error %d when attempting to bind %s to index %d", error, binding.name.c_str(), binding.index);
				break;
			}
		}
	}

	test_gl(glLinkProgram, shader_program_);

	GLint did_link = 0;
	test_gl(glGetProgramiv, shader_program_, GL_LINK_STATUS, &did_link);
	if(did_link == GL_FALSE) {
		if constexpr (Logger::ErrorsEnabled) {
			GLint log_length;
			test_gl(glGetProgramiv, shader_program_, GL_INFO_LOG_LENGTH, &log_length);
			if(log_length > 0) {
				std::vector<GLchar> log(log_length);
				test_gl(glGetProgramInfoLog, shader_program_, log_length, &log_length, log.data());
				Logger::error().append("Link log: %s", log.data());
			}
		}

		throw ProgramLinkageError;
	}
}

Shader::~Shader() {
	if(bound_shader == this) Shader::unbind();
	glDeleteProgram(shader_program_);
}

void Shader::bind() const {
	if(bound_shader != this) {
		test_gl(glUseProgram, shader_program_);
		bound_shader = this;
	}
	flush_functions();
}

void Shader::unbind() {
	bound_shader = nullptr;
	test_gl(glUseProgram, 0);
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
		test_gl(glEnableVertexAttribArray, GLuint(location));
		test_gl(glVertexAttribPointer, GLuint(location), size, type, normalised, stride, pointer);
		test_gl(glVertexAttribDivisor, GLuint(location), divisor);
	} else {
		Logger::error().append("Couldn't enable vertex attribute %s", name.c_str());
	}
}

// The various set_uniforms...
#define with_location(func, ...) {\
		const GLint location = glGetUniformLocation(shader_program_, name.c_str());	\
		if(location == -1) { \
			Logger::error().append("Couldn't get location for uniform %s", name.c_str());	\
		} else { \
			func(location, __VA_ARGS__);	\
			if(glGetError()) Logger::error().append("Error setting uniform %s via %s", name.c_str(), #func);	\
		} \
	}

void Shader::set_uniform(const std::string &name, const GLint value) {
	enqueue_function([name, value, this] {
		with_location(glUniform1i, value);
	});
}

void Shader::set_uniform(const std::string &name, const GLuint value) {
	enqueue_function([name, value, this] {
		with_location(glUniform1ui, value);
	});
}

void Shader::set_uniform(const std::string &name, const GLfloat value) {
	enqueue_function([name, value, this] {
		with_location(glUniform1f, value);
	});
}


void Shader::set_uniform(const std::string &name, const GLint value1, const GLint value2) {
	enqueue_function([name, value1, value2, this] {
		with_location(glUniform2i, value1, value2);
	});
}

void Shader::set_uniform(const std::string &name, const GLfloat value1, const GLfloat value2) {
	enqueue_function([name, value1, value2, this] {
		with_location(glUniform2f, value1, value2);
	});
}

void Shader::set_uniform(const std::string &name, const GLuint value1, const GLuint value2) {
	enqueue_function([name, value1, value2, this] {
		with_location(glUniform2ui, value1, value2);
	});
}

void Shader::set_uniform(const std::string &name, const GLint value1, const GLint value2, const GLint value3) {
	enqueue_function([name, value1, value2, value3, this] {
		with_location(glUniform3i, value1, value2, value3);
	});
}

void Shader::set_uniform(const std::string &name, const GLfloat value1, const GLfloat value2, const GLfloat value3) {
	enqueue_function([name, value1, value2, value3, this] {
		with_location(glUniform3f, value1, value2, value3);
	});
}

void Shader::set_uniform(const std::string &name, const GLuint value1, const GLuint value2, const GLuint value3) {
	enqueue_function([name, value1, value2, value3, this] {
		with_location(glUniform3ui, value1, value2, value3);
	});
}

void Shader::set_uniform(
	const std::string &name,
	const GLint value1,
	const GLint value2,
	const GLint value3,
	const GLint value4
) {
	enqueue_function([name, value1, value2, value3, value4, this] {
		with_location(glUniform4i, value1, value2, value3, value4);
	});
}

void Shader::set_uniform(
	const std::string &name,
	const GLfloat value1,
	const GLfloat value2,
	const GLfloat value3,
	const GLfloat value4
) {
	enqueue_function([name, value1, value2, value3, value4, this] {
		with_location(glUniform4f, value1, value2, value3, value4);
	});
}

void Shader::set_uniform(
	const std::string &name,
	const GLuint value1,
	const GLuint value2,
	const GLuint value3,
	const GLuint value4
) {
	enqueue_function([name, value1, value2, value3, value4, this] {
		with_location(glUniform4ui, value1, value2, value3, value4);
	});
}

void Shader::set_uniform(
	const std::string &name,
	const GLint size,
	const GLsizei count,
	const GLint *const values
) {
	std::size_t number_of_values = std::size_t(count) * std::size_t(size);
	std::vector<GLint> values_copy(values, values + number_of_values);

	enqueue_function([name, size, count, values_copy, this] {
		switch(size) {
			case 1: with_location(glUniform1iv, count, values_copy.data());	break;
			case 2: with_location(glUniform2iv, count, values_copy.data());	break;
			case 3: with_location(glUniform3iv, count, values_copy.data());	break;
			case 4: with_location(glUniform4iv, count, values_copy.data());	break;
		}
	});
}

void Shader::set_uniform(const std::string &name, const GLint size, const GLsizei count, const GLfloat *const values) {
	std::size_t number_of_values = std::size_t(count) * std::size_t(size);
	std::vector<GLfloat> values_copy(values, values + number_of_values);

	enqueue_function([name, size, count, values_copy, this] {
		switch(size) {
			case 1: with_location(glUniform1fv, count, values_copy.data());	break;
			case 2: with_location(glUniform2fv, count, values_copy.data());	break;
			case 3: with_location(glUniform3fv, count, values_copy.data());	break;
			case 4: with_location(glUniform4fv, count, values_copy.data());	break;
		}
	});
}

void Shader::set_uniform(const std::string &name, const GLint size, const GLsizei count, const GLuint *const values) {
	std::size_t number_of_values = std::size_t(count) * std::size_t(size);
	std::vector<GLuint> values_copy(values, values + number_of_values);

	enqueue_function([name, size, count, values_copy, this] {
		switch(size) {
			case 1: with_location(glUniform1uiv, count, values_copy.data());	break;
			case 2: with_location(glUniform2uiv, count, values_copy.data());	break;
			case 3: with_location(glUniform3uiv, count, values_copy.data());	break;
			case 4: with_location(glUniform4uiv, count, values_copy.data());	break;
		}
	});
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
	std::size_t number_of_values = std::size_t(count) * std::size_t(size) * std::size_t(size);
	std::vector<GLfloat> values_copy(values, values + number_of_values);

	enqueue_function([name, size, count, transpose, values_copy, this] {
		GLboolean glTranspose = transpose ? GL_TRUE : GL_FALSE;
		switch(size) {
			case 2: with_location(glUniformMatrix2fv, count, glTranspose, values_copy.data());	break;
			case 3: with_location(glUniformMatrix3fv, count, glTranspose, values_copy.data());	break;
			case 4: with_location(glUniformMatrix4fv, count, glTranspose, values_copy.data());	break;
		}
	});
}

void Shader::enqueue_function(const std::function<void(void)> function) {
	const std::lock_guard function_guard(function_mutex_);
	enqueued_functions_.push_back(function);
}

void Shader::flush_functions() const {
	const std::lock_guard function_guard(function_mutex_);
	for(std::function<void(void)> &function : enqueued_functions_) {
		function();
		test_gl_error();
	}
	enqueued_functions_.clear();
}
