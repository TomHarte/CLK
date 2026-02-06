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
	std::swap(attributes_, rhs.attributes_);

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
		test_gl([&]{ glVertexAttribDivisor(GLuint(location), divisor); });

		attributes_.emplace_back(VertexArrayAttribute{
			location, size, type, normalised, stride, pointer, divisor
		}).apply(0);
	} else {
		Logger::error().append("Couldn't enable vertex attribute %s", name.c_str());
	}
}

void Shader::set_vertex_attribute_offset(const size_t offset) {
	bind();
	for(const auto &attribute: attributes_) {
		attribute.apply(offset);
	}
}

void Shader::VertexArrayAttribute::apply(const size_t offset) const {
	test_gl([&]{
		glVertexAttribPointer(
			location,
			size,
			type,
			normalised,
			stride,
			reinterpret_cast<const uint8_t *>(pointer) + offset * stride
		);
	});
}

template <typename FuncT>
requires std::invocable<FuncT, GLint>
void Shader::with_location(const std::string &name, FuncT &&function) {
	const GLint location = glGetUniformLocation(shader_program_, name.c_str());
	if(location == -1) {
		Logger::error().append("Couldn't get location for uniform %s", name.c_str());
	} else {
		bind();
		function(location);
		if(glGetError()) Logger::error().append("Error setting uniform %s", name.c_str());
	}
}

void Shader::set_uniform(const std::string &name, const GLint value) {
	with_location(name, [&](const GLint location) { glUniform1i(location, value); });
}

void Shader::set_uniform(const std::string &name, const GLuint value) {
	with_location(name, [&](const GLint location) { glUniform1ui(location, value); });
}

void Shader::set_uniform(const std::string &name, const GLfloat value) {
	with_location(name, [&](const GLint location) { glUniform1f(location, value); });
}


void Shader::set_uniform(const std::string &name, const GLint value1, const GLint value2) {
	with_location(name, [&](const GLint location) { glUniform2i(location, value1, value2); });
}

void Shader::set_uniform(const std::string &name, const GLfloat value1, const GLfloat value2) {
	with_location(name, [&](const GLint location) { glUniform2f(location, value1, value2); });
}

void Shader::set_uniform(const std::string &name, const GLuint value1, const GLuint value2) {
	with_location(name, [&](const GLint location) { glUniform2ui(location, value1, value2); });
}

void Shader::set_uniform(const std::string &name, const GLint value1, const GLint value2, const GLint value3) {
	with_location(name, [&](const GLint location) { glUniform3i(location, value1, value2, value3); });
}

void Shader::set_uniform(const std::string &name, const GLfloat value1, const GLfloat value2, const GLfloat value3) {
	with_location(name, [&](const GLint location) { glUniform3f(location, value1, value2, value3); });
}

void Shader::set_uniform(const std::string &name, const GLuint value1, const GLuint value2, const GLuint value3) {
	with_location(name, [&](const GLint location) { glUniform3ui(location, value1, value2, value3); });
}

void Shader::set_uniform(
	const std::string &name,
	const GLint value1,
	const GLint value2,
	const GLint value3,
	const GLint value4
) {
	with_location(name, [&](const GLint location) { glUniform4i(location, value1, value2, value3, value4); });
}

void Shader::set_uniform(
	const std::string &name,
	const GLfloat value1,
	const GLfloat value2,
	const GLfloat value3,
	const GLfloat value4
) {
	with_location(name, [&](const GLint location) { glUniform4f(location, value1, value2, value3, value4); });
}

void Shader::set_uniform(
	const std::string &name,
	const GLuint value1,
	const GLuint value2,
	const GLuint value3,
	const GLuint value4
) {
	with_location(name, [&](const GLint location) { glUniform4ui(location, value1, value2, value3, value4); });
}

void Shader::set_uniform(
	const std::string &name,
	const GLint size,
	const GLsizei count,
	const GLint *const values
) {
	with_location(name, [&](const GLint location) {
		switch(size) {
			case 1: glUniform1iv(location, count, values);	break;
			case 2: glUniform2iv(location, count, values);	break;
			case 3: glUniform3iv(location, count, values);	break;
			case 4: glUniform4iv(location, count, values);	break;
		}
	});
}

void Shader::set_uniform(const std::string &name, const GLint size, const GLsizei count, const GLfloat *const values) {
	with_location(name, [&](const GLint location) {
		switch(size) {
			case 1: glUniform1fv(location, count, values);	break;
			case 2: glUniform2fv(location, count, values);	break;
			case 3: glUniform3fv(location, count, values);	break;
			case 4: glUniform4fv(location, count, values);	break;
		}
	});
}

void Shader::set_uniform(const std::string &name, const GLint size, const GLsizei count, const GLuint *const values) {
	with_location(name, [&](const GLint location) {
		switch(size) {
			case 1: glUniform1uiv(location, count, values);	break;
			case 2: glUniform2uiv(location, count, values);	break;
			case 3: glUniform3uiv(location, count, values);	break;
			case 4: glUniform4uiv(location, count, values);	break;
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
	with_location(name, [&](const GLint location) {
	const GLboolean glTranspose = transpose ? GL_TRUE : GL_FALSE;
		switch(size) {
			case 2: glUniformMatrix2fv(location, count, glTranspose, values);	break;
			case 3: glUniformMatrix3fv(location, count, glTranspose, values);	break;
			case 4: glUniformMatrix4fv(location, count, glTranspose, values);	break;
		}
	});
}
