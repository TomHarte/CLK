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
