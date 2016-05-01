//
//  TextureTarget.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/02/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "TextureTarget.hpp"
#include <math.h>

using namespace OpenGL;

TextureTarget::TextureTarget(GLsizei width, GLsizei height, GLenum texture_unit) :
	_width(width),
	_height(height),
	_pixel_shader(nullptr),
	_drawing_vertex_array(0),
	_drawing_array_buffer(0),
	_set_aspect_ratio(0.0f),
	_texture_unit(texture_unit)
{
	glGenFramebuffers(1, &_framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, _framebuffer);

	_expanded_width = 1 << (GLsizei)ceil(log2(width));
	_expanded_height = 1 << (GLsizei)ceil(log2(height));

	glGenTextures(1, &_texture);
	glActiveTexture(texture_unit);
	glBindTexture(GL_TEXTURE_2D, _texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)_expanded_width, (GLsizei)_expanded_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _texture, 0);

	if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		throw ErrorFramebufferIncomplete;
}

TextureTarget::~TextureTarget()
{
	glDeleteFramebuffers(1, &_framebuffer);
	glDeleteTextures(1, &_texture);
	if(_drawing_vertex_array) glDeleteVertexArrays(1, &_drawing_vertex_array);
	if(_drawing_array_buffer) glDeleteBuffers(1, &_drawing_array_buffer);
}

void TextureTarget::bind_framebuffer()
{
	glBindFramebuffer(GL_FRAMEBUFFER, _framebuffer);
	glViewport(0, 0, _width, _height);
}

void TextureTarget::bind_texture()
{
	glBindTexture(GL_TEXTURE_2D, _texture);
}

void TextureTarget::draw(float aspect_ratio)
{
	if(!_pixel_shader)
	{
		const char *vertex_shader =
			"#version 150\n"

			"in vec2 texCoord;"
			"in vec2 position;"

			"out vec2 texCoordVarying;"

			"void main(void)"
			"{"
				"texCoordVarying = texCoord;"
				"gl_Position = vec4(position, 0.0, 1.0);"
			"}";

		const char *fragment_shader =
			"#version 150\n"

			"in vec2 texCoordVarying;"
			"uniform sampler2D texID;"
			"out vec4 fragColour;"

			"void main(void)"
			"{"
				"fragColour = texture(texID, texCoordVarying);"
			"}";
		_pixel_shader = std::unique_ptr<Shader>(new Shader(vertex_shader, fragment_shader, nullptr));

		glGenVertexArrays(1, &_drawing_vertex_array);
		glGenBuffers(1, &_drawing_array_buffer);

		glBindVertexArray(_drawing_vertex_array);
		glBindBuffer(GL_ARRAY_BUFFER, _drawing_array_buffer);

		GLint positionAttribute			= _pixel_shader->get_attrib_location("position");
		GLint texCoordAttribute			= _pixel_shader->get_attrib_location("texCoord");

		glEnableVertexAttribArray((GLuint)positionAttribute);
		glEnableVertexAttribArray((GLuint)texCoordAttribute);

		const GLsizei vertexStride = 12;
		glVertexAttribPointer((GLuint)positionAttribute,	2, GL_FLOAT,	GL_FALSE,	vertexStride, (void *)0);
		glVertexAttribPointer((GLuint)texCoordAttribute,	2, GL_BYTE,		GL_FALSE,	vertexStride, (void *)(2 * sizeof(GLfloat)));

		GLint texIDUniform = _pixel_shader->get_uniform_location("texID");
		glUniform1i(texIDUniform, (GLint)(_texture_unit - GL_TEXTURE0));
	}

	if(_set_aspect_ratio != aspect_ratio)
	{
		aspect_ratio = _set_aspect_ratio;
		int8_t buffer[12*4];

		// establish texture coordinates
		buffer[8] = 0;		buffer[9] = 0;
		buffer[20] = 0;		buffer[21] = 1;
		buffer[32] = 1;		buffer[33] = 0;
		buffer[44] = 1;		buffer[45] = 1;

		// determine positions
		float internal_aspect_ratio = (float)_width / (float)_height;
		float aspect_ratio_ratio = internal_aspect_ratio / aspect_ratio;
		float *fl_buffer = (float *)buffer;
		if(aspect_ratio_ratio >= 1.0f)
		{
			// output is thinner than we are; letterbox
			fl_buffer[0] = -1.0f;	fl_buffer[1] = 1.0f / aspect_ratio_ratio;
			fl_buffer[3] = -1.0f;	fl_buffer[4] = -1.0f / aspect_ratio_ratio;
			fl_buffer[6] = 1.0f;	fl_buffer[7] = 1.0f / aspect_ratio_ratio;
			fl_buffer[9] = 1.0f;	fl_buffer[10] = -1.0f / aspect_ratio_ratio;
		}
		else
		{
			// output is wider than we are; pillarbox
			fl_buffer[0] = -aspect_ratio_ratio;	fl_buffer[1] = 1.0f;
			fl_buffer[3] = -aspect_ratio_ratio;	fl_buffer[4] = -1.0f;
			fl_buffer[6] = aspect_ratio_ratio;	fl_buffer[7] = 1.0f;
			fl_buffer[9] = aspect_ratio_ratio;	fl_buffer[10] = -1.0f;
		}

		// upload buffer
		glBindBuffer(GL_ARRAY_BUFFER, _drawing_array_buffer);
		glBufferData(GL_ARRAY_BUFFER, sizeof(buffer), buffer, GL_STATIC_DRAW);
	}

	glBindVertexArray(_drawing_vertex_array);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}
