//
//  Shader.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/02/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Shader.hpp"

#include <stdlib.h>
#include <stdio.h>

using namespace OpenGL;

namespace {
	Shader *bound_shader = nullptr;
}

GLuint Shader::compile_shader(const char *source, GLenum type)
{
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &source, NULL);
	glCompileShader(shader);

#if defined(DEBUG)
	GLint isCompiled = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
	if(isCompiled == GL_FALSE)
	{
		GLint logLength;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
		if(logLength > 0) {
			GLchar *log = (GLchar *)malloc((size_t)logLength);
			glGetShaderInfoLog(shader, logLength, &logLength, log);
			printf("Compile log:\n%s\n", log);
			free(log);
		}

		throw (type == GL_VERTEX_SHADER) ? VertexShaderCompilationError : FragmentShaderCompilationError;
	}
#endif

	return shader;
}

Shader::Shader(const char *vertex_shader, const char *fragment_shader, const AttributeBinding *attribute_bindings)
{
	_shader_program = glCreateProgram();
	GLuint vertex = compile_shader(vertex_shader, GL_VERTEX_SHADER);
	GLuint fragment = compile_shader(fragment_shader, GL_FRAGMENT_SHADER);

	glAttachShader(_shader_program, vertex);
	glAttachShader(_shader_program, fragment);

	if(attribute_bindings)
	{
		while(attribute_bindings->name)
		{
			glBindAttribLocation(_shader_program, attribute_bindings->index, attribute_bindings->name);
			attribute_bindings++;
		}
	}

	glLinkProgram(_shader_program);

#if defined(DEBUG)
	GLint didLink = 0;
	glGetProgramiv(_shader_program, GL_LINK_STATUS, &didLink);
	if(didLink == GL_FALSE)
	{
		GLint logLength;
		glGetProgramiv(_shader_program, GL_INFO_LOG_LENGTH, &logLength);
		if(logLength > 0) {
			GLchar *log = (GLchar *)malloc((size_t)logLength);
			glGetProgramInfoLog(_shader_program, logLength, &logLength, log);
			printf("Link log:\n%s\n", log);
			free(log);
		}
		throw ProgramLinkageError;
	}
#endif
}

Shader::~Shader()
{
	if(bound_shader == this) Shader::unbind();
	glDeleteProgram(_shader_program);
}

void Shader::bind()
{
	if(bound_shader != this)
	{
		glUseProgram(_shader_program);
		bound_shader = this;
	}
}

void Shader::unbind()
{
	bound_shader = nullptr;
	glUseProgram(0);
}

GLint Shader::get_attrib_location(const GLchar *name)
{
	return glGetAttribLocation(_shader_program, name);
}

GLint Shader::get_uniform_location(const GLchar *name)
{
	return glGetUniformLocation(_shader_program, name);
}

void Shader::enable_vertex_attribute_with_pointer(const char *name, GLint size, GLenum type, GLboolean normalised, GLsizei stride, const GLvoid *pointer, GLuint divisor)
{
	GLint location = get_attrib_location(name);
	glEnableVertexAttribArray((GLuint)location);
	glVertexAttribPointer((GLuint)location, size, type, normalised, stride, pointer);
	glVertexAttribDivisor((GLuint)location, divisor);
}

// The various set_uniforms...
GLint Shader::location_for_bound_uniform(const GLchar *name)
{
	bind();
	return glGetUniformLocation(_shader_program, name);
}

void Shader::set_uniform(const GLchar *name, GLint value)
{
	glUniform1i(location_for_bound_uniform(name), value);
}

void Shader::set_uniform(const GLchar *name, GLint value1, GLint value2)
{
	glUniform2i(location_for_bound_uniform(name), value1, value2);
}

void Shader::set_uniform(const GLchar *name, GLint value1, GLint value2, GLint value3)
{
	glUniform3i(location_for_bound_uniform(name), value1, value2, value3);
}

void Shader::set_uniform(const GLchar *name, GLint value1, GLint value2, GLint value3, GLint value4)
{
	glUniform4i(location_for_bound_uniform(name), value1, value2, value3, value4);
}

void Shader::set_uniform(const GLchar *name, GLint size, GLsizei count, const GLint *values)
{
	switch(size)
	{
		case 1: glUniform1iv(location_for_bound_uniform(name), count, values);	break;
		case 2: glUniform2iv(location_for_bound_uniform(name), count, values);	break;
		case 3: glUniform3iv(location_for_bound_uniform(name), count, values);	break;
		case 4: glUniform4iv(location_for_bound_uniform(name), count, values);	break;
	}
}

void Shader::set_uniform(const GLchar *name, GLfloat value)
{
	glUniform1f(location_for_bound_uniform(name), value);
}

void Shader::set_uniform(const GLchar *name, GLfloat value1, GLfloat value2)
{
	glUniform2f(location_for_bound_uniform(name), value1, value2);
}

void Shader::set_uniform(const GLchar *name, GLfloat value1, GLfloat value2, GLfloat value3)
{
	glUniform3f(location_for_bound_uniform(name), value1, value2, value3);
}

void Shader::set_uniform(const GLchar *name, GLfloat value1, GLfloat value2, GLfloat value3, GLfloat value4)
{
	glUniform4f(location_for_bound_uniform(name), value1, value2, value3, value4);
}

void Shader::set_uniform(const GLchar *name, GLint size, GLsizei count, const GLfloat *values)
{
	switch(size)
	{
		case 1: glUniform1fv(location_for_bound_uniform(name), count, values);	break;
		case 2: glUniform2fv(location_for_bound_uniform(name), count, values);	break;
		case 3: glUniform3fv(location_for_bound_uniform(name), count, values);	break;
		case 4: glUniform4fv(location_for_bound_uniform(name), count, values);	break;
	}
}

void Shader::set_uniform(const GLchar *name, GLuint value)
{
	glUniform1ui(location_for_bound_uniform(name), value);
}

void Shader::set_uniform(const GLchar *name, GLuint value1, GLuint value2)
{
	glUniform2ui(location_for_bound_uniform(name), value1, value2);
}

void Shader::set_uniform(const GLchar *name, GLuint value1, GLuint value2, GLuint value3)
{
	glUniform3ui(location_for_bound_uniform(name), value1, value2, value3);
}

void Shader::set_uniform(const GLchar *name, GLuint value1, GLuint value2, GLuint value3, GLuint value4)
{
	glUniform4ui(location_for_bound_uniform(name), value1, value2, value3, value4);
}

void Shader::set_uniform(const GLchar *name, GLint size, GLsizei count, const GLuint *values)
{
	switch(size)
	{
		case 1: glUniform1uiv(location_for_bound_uniform(name), count, values);	break;
		case 2: glUniform2uiv(location_for_bound_uniform(name), count, values);	break;
		case 3: glUniform3uiv(location_for_bound_uniform(name), count, values);	break;
		case 4: glUniform4uiv(location_for_bound_uniform(name), count, values);	break;
	}
}

void Shader::set_uniform_matrix(const GLchar *name, GLint size, bool transpose, const GLfloat *values)
{
	set_uniform_matrix(name, size, 1, transpose, values);
}

void Shader::set_uniform_matrix(const GLchar *name, GLint size, GLsizei count, bool transpose, const GLfloat *values)
{
	GLboolean glTranspose = transpose ? GL_TRUE : GL_FALSE;
	switch(size)
	{
		case 2: glUniformMatrix2fv(location_for_bound_uniform(name), count, glTranspose, values);	break;
		case 3: glUniformMatrix3fv(location_for_bound_uniform(name), count, glTranspose, values);	break;
		case 4: glUniformMatrix4fv(location_for_bound_uniform(name), count, glTranspose, values);	break;
	}
}
