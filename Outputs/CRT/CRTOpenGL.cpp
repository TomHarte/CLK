
//  CRTOpenGL.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/02/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "CRT.hpp"
#include <stdlib.h>

// TODO: figure out correct include paths for other platforms.
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>

using namespace Outputs;

struct CRT::OpenGLState {
	GLuint vertexShader, fragmentShader;
	GLuint shaderProgram;
	GLuint arrayBuffer, vertexArray;

	GLint positionAttribute;
	GLint textureCoordinatesAttribute;
	GLint lateralAttribute;

	GLint textureSizeUniform, windowSizeUniform;
	GLint boundsOriginUniform, boundsSizeUniform;
	GLint alphaUniform;

	GLuint textureName, shadowMaskTextureName;

	CRTSize textureSize;
};

static GLenum formatForDepth(unsigned int depth)
{
	switch(depth)
	{
		default: return -1;
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

	if(!_current_frame)
	{
		glClear(GL_COLOR_BUFFER_BIT);
	}
	else
	{
		if(_current_frame != _last_drawn_frame)
		{
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
			}

			glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(_current_frame->number_of_vertices * _current_frame->size_per_vertex), _current_frame->vertices, GL_DYNAMIC_DRAW);

			glBindTexture(GL_TEXTURE_2D, _openGL_state->textureName);
			if(_openGL_state->textureSize.width != _current_frame->size.width || _openGL_state->textureSize.height != _current_frame->size.height)
			{
				GLenum format = formatForDepth(_current_frame->buffers[0].depth);
				glTexImage2D(GL_TEXTURE_2D, 0, (GLint)format, _current_frame->size.width, _current_frame->size.height, 0, format, GL_UNSIGNED_BYTE, _current_frame->buffers[0].data);
				_openGL_state->textureSize = _current_frame->size;

				if(_openGL_state->textureSizeUniform >= 0) glUniform2f(_openGL_state->textureSizeUniform, _current_frame->size.width, _current_frame->size.height);
			}
			else
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, _current_frame->dirty_size.width, _current_frame->dirty_size.height, formatForDepth(_current_frame->buffers[0].depth), GL_UNSIGNED_BYTE, _current_frame->buffers[0].data);
		}
	}

	if(_current_frame != _last_drawn_frame || only_if_dirty)
	{
		glClear(GL_COLOR_BUFFER_BIT);
		glDrawArrays(GL_TRIANGLES, 0, (GLsizei)_current_frame->number_of_vertices);
	}

	_current_frame_mutex->unlock();
}

void CRT::set_openGL_context_will_change(bool should_delete_resources)
{
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

	const char *const ntscVertexShaderGlobals =
		"out vec2 srcCoordinatesVarying[4];\n"
		"out float phase;\n";

	const char *const ntscVertexShaderBody =
		"phase = srcCoordinates.x * 6.283185308;\n"
		"\n"
		"srcCoordinatesVarying[0] = vec2(srcCoordinates.x / textureSize.x, (srcCoordinates.y + 0.5) / textureSize.y);\n"
		"srcCoordinatesVarying[3] = srcCoordinatesVarying[0] + vec2(0.375 / textureSize.x, 0.0);\n"
		"srcCoordinatesVarying[2] = srcCoordinatesVarying[0] + vec2(0.125 / textureSize.x, 0.0);\n"
		"srcCoordinatesVarying[1] = srcCoordinatesVarying[0] - vec2(0.125 / textureSize.x, 0.0);\n"
		"srcCoordinatesVarying[0] = srcCoordinatesVarying[0] - vec2(0.325 / textureSize.x, 0.0);\n";

//	const char *const rgbVertexShaderGlobals =
//		"out vec2 srcCoordinatesVarying[5];\n";

//	const char *const rgbVertexShaderBody =
//		"srcCoordinatesVarying[2] = vec2(srcCoordinates.x / textureSize.x, (srcCoordinates.y + 0.5) / textureSize.y);\n"
//		"srcCoordinatesVarying[0] = srcCoordinatesVarying[1] - vec2(1.0 / textureSize.x, 0.0);\n"
//		"srcCoordinatesVarying[1] = srcCoordinatesVarying[1] - vec2(0.5 / textureSize.x, 0.0);\n"
//		"srcCoordinatesVarying[3] = srcCoordinatesVarying[1] + vec2(0.5 / textureSize.x, 0.0);\n"
//		"srcCoordinatesVarying[4] = srcCoordinatesVarying[1] + vec2(1.0 / textureSize.x, 0.0);\n";

	const char *const vertexShader =
		"#version 150\n"
		"\n"
		"in vec2 position;\n"
		"in vec2 srcCoordinates;\n"
		"in float lateral;\n"
		"\n"
		"uniform vec2 boundsOrigin;\n"
		"uniform vec2 boundsSize;\n"
		"\n"
		"out float lateralVarying;\n"
		"out vec2 shadowMaskCoordinates;\n"
		"\n"
		"uniform vec2 textureSize;\n"
		"\n"
		"const float shadowMaskMultiple = 600;\n"
		"\n"
		"%@\n"
		"void main (void)\n"
		"{\n"
			"lateralVarying = lateral + 1.0707963267949;\n"
			"\n"
			"shadowMaskCoordinates = position * vec2(shadowMaskMultiple, shadowMaskMultiple * 0.85057471264368);\n"
			"\n"
			"%@\n"
			"\n"
			"vec2 mappedPosition = (position - boundsOrigin) / boundsSize;"
			"gl_Position = vec4(mappedPosition.x * 2.0 - 1.0, 1.0 - mappedPosition.y * 2.0, 0.0, 1.0);\n"
		"}\n";

	return nullptr;
// + mappedPosition.x / 131.0

//	switch(_signalType)
//	{
//		case CSCathodeRayViewSignalTypeNTSC: return [NSString stringWithFormat:vertexShader, ntscVertexShaderGlobals, ntscVertexShaderBody];
//		case CSCathodeRayViewSignalTypeRGB:	 return [NSString stringWithFormat:vertexShader, rgbVertexShaderGlobals, rgbVertexShaderBody];
//	}
}

char *CRT::get_fragment_shader()
{
	// assumes y = [0, 1], i and q = [-0.5, 0.5]; therefore i components are multiplied by 1.1914 versus standard matrices, q by 1.0452
	const char *const yiqToRGB = "const mat3 yiqToRGB = mat3(1.0, 1.0, 1.0, 1.1389784, -0.3240608, -1.3176884, 0.6490692, -0.6762444, 1.7799756);";

	// assumes y = [0,1], u and v = [-0.5, 0.5]; therefore u components are multiplied by 1.14678899082569, v by 0.8130081300813
	const char *const yuvToRGB = "const mat3 yiqToRGB = mat3(1.0, 1.0, 1.0, 0.0, -0.75213899082569, 2.33040137614679, 0.92669105691057, -0.4720325203252, 0.0);";

	const char *const fragmentShader =
		"#version 150\n"
		"\n"
		"in float lateralVarying;\n"
		"in vec2 shadowMaskCoordinates;\n"
		"out vec4 fragColour;\n"
		"\n"
		"uniform sampler2D texID;\n"
		"uniform sampler2D shadowMaskTexID;\n"
		"uniform float alpha;\n"
		"\n"
		"in vec2 srcCoordinatesVarying[4];\n"
		"in float phase;\n"
		"%@\n"
		"%@\n"
		"\n"
		"void main(void)\n"
		"{\n"
			"%@\n"
		"}\n";

	const char *const ntscFragmentShaderGlobals =
		"in vec2 srcCoordinatesVarying[4];\n"
		"in float phase;\n"
		"\n"
		"// for conversion from i and q are in the range [-0.5, 0.5] (so i needs to be multiplied by 1.1914 and q by 1.0452)\n"
		"const mat3 yiqToRGB = mat3(1.0, 1.0, 1.0, 1.1389784, -0.3240608, -1.3176884, 0.6490692, -0.6762444, 1.7799756);\n";

	const char *const ntscFragmentShaderBody =
		"vec4 angles = vec4(phase) + vec4(-2.35619449019234, -0.78539816339745, 0.78539816339745, 2.35619449019234);\n"
		"vec4 samples = vec4("
		"   sample(srcCoordinatesVarying[0], angles.x),"
		"	sample(srcCoordinatesVarying[1], angles.y),"
		"	sample(srcCoordinatesVarying[2], angles.z),"
		"	sample(srcCoordinatesVarying[3], angles.w)"
		");\n"
		"\n"
		"float y = dot(vec4(0.25), samples);\n"
		"samples -= vec4(y);\n"
		"\n"
		"float i = dot(cos(angles), samples);\n"
		"float q = dot(sin(angles), samples);\n"
		"\n"
		"fragColour = 5.0 * texture(shadowMaskTexID, shadowMaskCoordinates) * vec4(yiqToRGB * vec3(y, i, q), 1.0);//sin(lateralVarying));\n";

//	const char *const rgbFragmentShaderGlobals =
//		"in vec2 srcCoordinatesVarying[5];\n"; // texture(shadowMaskTexID, shadowMaskCoordinates) *

//	const char *const rgbFragmentShaderBody =
//		"fragColour =	sample(srcCoordinatesVarying[2]);";
//		@"fragColour =	(sample(srcCoordinatesVarying[0]) * -0.1) + \
//						(sample(srcCoordinatesVarying[1]) * 0.3) + \
//						(sample(srcCoordinatesVarying[2]) * 0.6) + \
//						(sample(srcCoordinatesVarying[3]) * 0.3) + \
//						(sample(srcCoordinatesVarying[4]) * -0.1);";

//		dot(vec3(1.0/6.0, 2.0/3.0, 1.0/6.0), vec3(sample(srcCoordinatesVarying[0]), sample(srcCoordinatesVarying[0]), sample(srcCoordinatesVarying[0])));//sin(lateralVarying));\n";

	return nullptr;

//	switch(_signalType)
//	{
//		case CSCathodeRayViewSignalTypeNTSC: return [NSString stringWithFormat:fragmentShader, ntscFragmentShaderGlobals, ntscFragmentShaderBody];
//		case CSCathodeRayViewSignalTypeRGB:	 return [NSString stringWithFormat:fragmentShader, rgbFragmentShaderGlobals, rgbFragmentShaderBody];
//	}
}
