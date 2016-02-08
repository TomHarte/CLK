
//  CRTOpenGL.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/02/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "CRT.hpp"
#include <stdlib.h>

#include "OpenGL.hpp"
#include "TextureTarget.hpp"
#include "Shader.hpp"

using namespace Outputs;

struct CRT::OpenGLState {
	OpenGL::Shader *shaderProgram;
	GLuint arrayBuffer, vertexArray;

	GLint positionAttribute;
	GLint textureCoordinatesAttribute;
	GLint lateralAttribute;

	GLint textureSizeUniform, windowSizeUniform;
	GLint boundsOriginUniform, boundsSizeUniform;
	GLint alphaUniform;

	GLuint textureName, shadowMaskTextureName;

	CRTSize textureSize;

	OpenGL::TextureTarget *compositeTexture;
	OpenGL::TextureTarget *colourTexture;
	OpenGL::TextureTarget *filteredTexture;

	OpenGLState() : shaderProgram(nullptr) {}
	~OpenGLState()
	{
		delete shaderProgram;
	}
};

static GLenum formatForDepth(unsigned int depth)
{
	switch(depth)
	{
		default: return GL_FALSE;
		case 1: return GL_RED;
		case 2: return GL_RG;
		case 3: return GL_RGB;
		case 4: return GL_RGBA;
	}
}

void CRT::construct_openGL()
{
	_openGL_state = nullptr;
	_current_frame = _last_drawn_frame = nullptr;
	_composite_shader = _rgb_shader = nullptr;
}

void CRT::destruct_openGL()
{
	delete (OpenGLState *)_openGL_state;
	if(_composite_shader) free(_composite_shader);
	if(_rgb_shader) free(_rgb_shader);
}

void CRT::draw_frame(int output_width, int output_height, bool only_if_dirty)
{
	_current_frame_mutex->lock();

	GLint defaultFramebuffer;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &defaultFramebuffer);

	if(!_current_frame && !only_if_dirty)
	{
		glClear(GL_COLOR_BUFFER_BIT);
	}

	if(_current_frame && (_current_frame != _last_drawn_frame || !only_if_dirty))
	{
		glClear(GL_COLOR_BUFFER_BIT);

		if(!_openGL_state)
		{
			_openGL_state = new OpenGLState;

			glGenTextures(1, &_openGL_state->textureName);
			glBindTexture(GL_TEXTURE_2D, _openGL_state->textureName);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

			glGenVertexArrays(1, &_openGL_state->vertexArray);
			glBindVertexArray(_openGL_state->vertexArray);
			glGenBuffers(1, &_openGL_state->arrayBuffer);
			glBindBuffer(GL_ARRAY_BUFFER, _openGL_state->arrayBuffer);

			prepare_shader();
		}

		push_size_uniforms(output_width, output_height);

		glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(_current_frame->number_of_vertices * _current_frame->size_per_vertex), _current_frame->vertices, GL_DYNAMIC_DRAW);

		glBindTexture(GL_TEXTURE_2D, _openGL_state->textureName);
		if(_openGL_state->textureSize.width != _current_frame->size.width || _openGL_state->textureSize.height != _current_frame->size.height)
		{
			GLenum format = formatForDepth(_current_frame->buffers[0].depth);
			glTexImage2D(GL_TEXTURE_2D, 0, (GLint)format, _current_frame->size.width, _current_frame->size.height, 0, format, GL_UNSIGNED_BYTE, _current_frame->buffers[0].data);
			_openGL_state->textureSize = _current_frame->size;

			if(_openGL_state->textureSizeUniform >= 0)
				glUniform2f(_openGL_state->textureSizeUniform, _current_frame->size.width, _current_frame->size.height);
		}
		else
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, _current_frame->size.width, _current_frame->dirty_size.height, formatForDepth(_current_frame->buffers[0].depth), GL_UNSIGNED_BYTE, _current_frame->buffers[0].data);

		glDrawArrays(GL_TRIANGLES, 0, (GLsizei)_current_frame->number_of_vertices);
	}

	_current_frame_mutex->unlock();
}

void CRT::set_openGL_context_will_change(bool should_delete_resources)
{
}

void CRT::push_size_uniforms(unsigned int output_width, unsigned int output_height)
{
	if(_openGL_state->windowSizeUniform >= 0)
	{
		glUniform2f(_openGL_state->windowSizeUniform, output_width, output_height);
	}

//	GLfloat outputAspectRatioMultiplier = 1.0;//(viewSize.x / viewSize.y) / (4.0 / 3.0);

//	_aspectRatioCorrectedBounds = _frameBounds;

//	CGFloat bonusWidth = (outputAspectRatioMultiplier - 1.0f) * _frameBounds.size.width;
//	_aspectRatioCorrectedBounds.origin.x -= bonusWidth * 0.5f * _aspectRatioCorrectedBounds.size.width;
//	_aspectRatioCorrectedBounds.size.width *= outputAspectRatioMultiplier;

	if(_openGL_state->boundsOriginUniform >= 0)
		glUniform2f(_openGL_state->boundsOriginUniform, 0.0, 0.0); //(GLfloat)_aspectRatioCorrectedBounds.origin.x, (GLfloat)_aspectRatioCorrectedBounds.origin.y);

	if(_openGL_state->boundsSizeUniform >= 0)
		glUniform2f(_openGL_state->boundsSizeUniform, 1.0, 1.0);//(GLfloat)_aspectRatioCorrectedBounds.size.width, (GLfloat)_aspectRatioCorrectedBounds.size.height);
}

void CRT::set_composite_sampling_function(const char *shader)
{
	_composite_shader = strdup(shader);
}

void CRT::set_rgb_sampling_function(const char *shader)
{
	_rgb_shader = strdup(shader);
}

char *CRT::get_vertex_shader()
{
	// the main job of the vertex shader is just to map from an input area of [0,1]x[0,1], with the origin in the
	// top left to OpenGL's [-1,1]x[-1,1] with the origin in the lower left, and to convert input data coordinates
	// from integral to floating point; there's also some setup for NTSC, PAL or whatever.

//	const char *const ntscVertexShaderGlobals =
//		"out vec2 srcCoordinatesVarying[4];\n"
//		"out float phase;\n";
//
//	const char *const ntscVertexShaderBody =
//		"phase = srcCoordinates.x * 6.283185308;\n"
//		"\n"
//		"srcCoordinatesVarying[0] = vec2(srcCoordinates.x / textureSize.x, (srcCoordinates.y + 0.5) / textureSize.y);\n"
//		"srcCoordinatesVarying[3] = srcCoordinatesVarying[0] + vec2(0.375 / textureSize.x, 0.0);\n"
//		"srcCoordinatesVarying[2] = srcCoordinatesVarying[0] + vec2(0.125 / textureSize.x, 0.0);\n"
//		"srcCoordinatesVarying[1] = srcCoordinatesVarying[0] - vec2(0.125 / textureSize.x, 0.0);\n"
//		"srcCoordinatesVarying[0] = srcCoordinatesVarying[0] - vec2(0.325 / textureSize.x, 0.0);\n";

	return strdup(
		"#version 150\n"

		"in vec2 position;"
		"in vec2 srcCoordinates;"
		"in float lateral;"

		"uniform vec2 boundsOrigin;"
		"uniform vec2 boundsSize;"

		"out float lateralVarying;"
		"out vec2 shadowMaskCoordinates;"

		"uniform vec2 textureSize;"

		"const float shadowMaskMultiple = 600;"

		"out vec2 srcCoordinatesVarying;"

		"void main(void)"
		"{"
			"lateralVarying = lateral + 1.0707963267949;"

			"shadowMaskCoordinates = position * vec2(shadowMaskMultiple, shadowMaskMultiple * 0.85057471264368);"

			"srcCoordinatesVarying = vec2(srcCoordinates.x / textureSize.x, (srcCoordinates.y + 0.5) / textureSize.y);\n"

			"vec2 mappedPosition = (position - boundsOrigin) / boundsSize;"
			"gl_Position = vec4(mappedPosition.x * 2.0 - 1.0, 1.0 - mappedPosition.y * 2.0, 0.0, 1.0);"
		"}");
}

char *CRT::get_fragment_shader()
{
	// assumes y = [0, 1], i and q = [-0.5, 0.5]; therefore i components are multiplied by 1.1914 versus standard matrices, q by 1.0452
//	const char *const yiqToRGB = "const mat3 yiqToRGB = mat3(1.0, 1.0, 1.0, 1.1389784, -0.3240608, -1.3176884, 0.6490692, -0.6762444, 1.7799756);";

	// assumes y = [0,1], u and v = [-0.5, 0.5]; therefore u components are multiplied by 1.14678899082569, v by 0.8130081300813
//	const char *const yuvToRGB = "const mat3 yiqToRGB = mat3(1.0, 1.0, 1.0, 0.0, -0.75213899082569, 2.33040137614679, 0.92669105691057, -0.4720325203252, 0.0);";

//	const char *const ntscFragmentShaderGlobals =
//		"in vec2 srcCoordinatesVarying[4];\n"
//		"in float phase;\n"
//		"\n"
//		"// for conversion from i and q are in the range [-0.5, 0.5] (so i needs to be multiplied by 1.1914 and q by 1.0452)\n"
//		"const mat3 yiqToRGB = mat3(1.0, 1.0, 1.0, 1.1389784, -0.3240608, -1.3176884, 0.6490692, -0.6762444, 1.7799756);\n";

//	const char *const ntscFragmentShaderBody =
//		"vec4 angles = vec4(phase) + vec4(-2.35619449019234, -0.78539816339745, 0.78539816339745, 2.35619449019234);\n"
//		"vec4 samples = vec4("
//		"   sample(srcCoordinatesVarying[0], angles.x),"
//		"	sample(srcCoordinatesVarying[1], angles.y),"
//		"	sample(srcCoordinatesVarying[2], angles.z),"
//		"	sample(srcCoordinatesVarying[3], angles.w)"
//		");\n"
//		"\n"
//		"float y = dot(vec4(0.25), samples);\n"
//		"samples -= vec4(y);\n"
//		"\n"
//		"float i = dot(cos(angles), samples);\n"
//		"float q = dot(sin(angles), samples);\n"
//		"\n"
//		"fragColour = 5.0 * texture(shadowMaskTexID, shadowMaskCoordinates) * vec4(yiqToRGB * vec3(y, i, q), 1.0);//sin(lateralVarying));\n";

//		dot(vec3(1.0/6.0, 2.0/3.0, 1.0/6.0), vec3(sample(srcCoordinatesVarying[0]), sample(srcCoordinatesVarying[0]), sample(srcCoordinatesVarying[0])));//sin(lateralVarying));\n";

	return get_compound_shader(
		"#version 150\n"

		"in float lateralVarying;"
		"in vec2 shadowMaskCoordinates;"
		"out vec4 fragColour;"

		"uniform sampler2D texID;"
		"uniform sampler2D shadowMaskTexID;"
		"uniform float alpha;"

		"in vec2 srcCoordinatesVarying;"
		"in float phase;\n"
		"%s\n"

		"void main(void)"
		"{"
			"fragColour = vec4(rgb_sample(srcCoordinatesVarying).rgb, 1.0);"
		"}"
	, _rgb_shader);
}

char *CRT::get_compound_shader(const char *base, const char *insert)
{
	size_t totalLength = strlen(base) + strlen(insert) + 1;
	char *text = new char[totalLength];
	snprintf(text, totalLength, base, insert);
	return text;
}

void CRT::prepare_shader()
{
	char *vertex_shader = get_vertex_shader();
	char *fragment_shader = get_fragment_shader();

	_openGL_state->shaderProgram = new OpenGL::Shader(vertex_shader, fragment_shader);
	_openGL_state->shaderProgram->bind();

	_openGL_state->positionAttribute			= _openGL_state->shaderProgram->get_attrib_location("position");
	_openGL_state->textureCoordinatesAttribute	= _openGL_state->shaderProgram->get_attrib_location("srcCoordinates");
	_openGL_state->lateralAttribute				= _openGL_state->shaderProgram->get_attrib_location("lateral");
	_openGL_state->alphaUniform					= _openGL_state->shaderProgram->get_uniform_location("alpha");
	_openGL_state->textureSizeUniform			= _openGL_state->shaderProgram->get_uniform_location("textureSize");
	_openGL_state->windowSizeUniform			= _openGL_state->shaderProgram->get_uniform_location("windowSize");
	_openGL_state->boundsSizeUniform			= _openGL_state->shaderProgram->get_uniform_location("boundsSize");
	_openGL_state->boundsOriginUniform			= _openGL_state->shaderProgram->get_uniform_location("boundsOrigin");

	GLint texIDUniform				= _openGL_state->shaderProgram->get_uniform_location("texID");
	GLint shadowMaskTexIDUniform	= _openGL_state->shaderProgram->get_uniform_location("shadowMaskTexID");

	glUniform1i(texIDUniform, 0);
	glUniform1i(shadowMaskTexIDUniform, 1);

	glEnableVertexAttribArray((GLuint)_openGL_state->positionAttribute);
	glEnableVertexAttribArray((GLuint)_openGL_state->textureCoordinatesAttribute);
	glEnableVertexAttribArray((GLuint)_openGL_state->lateralAttribute);

	const GLsizei vertexStride = kCRTSizeOfVertex;
	glVertexAttribPointer((GLuint)_openGL_state->positionAttribute,			2, GL_UNSIGNED_SHORT,	GL_TRUE,	vertexStride, (void *)kCRTVertexOffsetOfPosition);
	glVertexAttribPointer((GLuint)_openGL_state->textureCoordinatesAttribute, 2, GL_UNSIGNED_SHORT,	GL_FALSE,	vertexStride, (void *)kCRTVertexOffsetOfTexCoord);
	glVertexAttribPointer((GLuint)_openGL_state->lateralAttribute,			1, GL_UNSIGNED_BYTE,	GL_FALSE,	vertexStride, (void *)kCRTVertexOffsetOfLateral);
}

void CRT::set_output_device(OutputDevice output_device)
{
	_output_device = output_device;
}
