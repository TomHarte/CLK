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
		if (logLength > 0) {
			GLchar *log = (GLchar *)malloc((size_t)logLength);
			glGetShaderInfoLog(shader, logLength, &logLength, log);
			printf("Compile log:\n%s\n", log);
			free(log);
		}
	}
#endif

	return shader;
}

Shader::Shader(const char *vertex_shader, const char *fragment_shader)
{
	_shader_program = glCreateProgram();
	GLuint vertex = compile_shader(vertex_shader, GL_VERTEX_SHADER);
	GLuint fragment = compile_shader(fragment_shader, GL_FRAGMENT_SHADER);

	glAttachShader(_shader_program, vertex);
	glAttachShader(_shader_program, fragment);
	glLinkProgram(_shader_program);

#if defined(DEBUG)
	GLint didLink = 0;
	glGetProgramiv(_shader_program, GL_LINK_STATUS, &didLink);
	if(didLink == GL_FALSE)
	{
		GLint logLength;
		glGetProgramiv(_shader_program, GL_INFO_LOG_LENGTH, &logLength);
		if (logLength > 0) {
			GLchar *log = (GLchar *)malloc((size_t)logLength);
			glGetProgramInfoLog(_shader_program, logLength, &logLength, log);
			printf("Link log:\n%s\n", log);
			free(log);
		}
	}
#endif
}

Shader::~Shader()
{
	// TODO: ensure this is destructed within the correct context.
//	glDeleteProgram(_shader_program);
}

void Shader::bind()
{
	glUseProgram(_shader_program);
}

GLint Shader::get_attrib_location(const char *name)
{
	return glGetAttribLocation(_shader_program, name);
}

GLint Shader::get_uniform_location(const char *name)
{
	return glGetUniformLocation(_shader_program, name);
}
