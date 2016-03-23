//  CRTOpenGL.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/02/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "CRT.hpp"
#include <stdlib.h>
#include <math.h>

#include "CRTOpenGL.hpp"
#include "../../../SignalProcessing/FIRFilter.hpp"

using namespace Outputs::CRT;

namespace {
	static const GLenum first_supplied_buffer_texture_unit = 3;
}

OpenGLOutputBuilder::OpenGLOutputBuilder(unsigned int buffer_depth) :
	_run_write_pointer(0),
	_output_mutex(new std::mutex),
	_visible_area(Rect(0, 0, 1, 1)),
	_composite_src_output_y(0),
	_composite_shader(nullptr),
	_rgb_shader(nullptr),
	_output_buffer_data(nullptr),
	_output_buffer_sync(nullptr),
	_input_texture_data(nullptr)
{
	_run_builders = new CRTRunBuilder *[NumberOfFields];
	for(int builder = 0; builder < NumberOfFields; builder++)
	{
		_run_builders[builder] = new CRTRunBuilder();
	}
//	_composite_src_runs = std::unique_ptr<CRTRunBuilder>(new CRTRunBuilder(InputVertexSize));

	_buffer_builder = std::unique_ptr<CRTInputBufferBuilder>(new CRTInputBufferBuilder(buffer_depth));
}

OpenGLOutputBuilder::~OpenGLOutputBuilder()
{
	for(int builder = 0; builder < NumberOfFields; builder++)
	{
		delete _run_builders[builder];
	}
	delete[] _run_builders;

	glUnmapBuffer(GL_ARRAY_BUFFER);
	glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
	glDeleteTextures(1, &textureName);
	glDeleteBuffers(1, &_input_texture_array);
	glDeleteBuffers(1, &output_array_buffer);
	glDeleteVertexArrays(1, &output_vertex_array);

	free(_composite_shader);
	free(_rgb_shader);
}

static GLenum formatForDepth(size_t depth)
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

void OpenGLOutputBuilder::draw_frame(unsigned int output_width, unsigned int output_height, bool only_if_dirty)
{
	// establish essentials
	if(!composite_input_shader_program && !rgb_shader_program)
	{
		// generate and bind texture for input data
		glGenTextures(1, &textureName);
		glActiveTexture(GL_TEXTURE0 + first_supplied_buffer_texture_unit);
		glBindTexture(GL_TEXTURE_2D, textureName);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

		GLenum format = formatForDepth(_buffer_builder->bytes_per_pixel);
		glGenBuffers(1, &_input_texture_array);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, _input_texture_array);
		_input_texture_array_size = (GLsizeiptr)(InputBufferBuilderWidth * InputBufferBuilderHeight * _buffer_builder->bytes_per_pixel);
		glBufferData(GL_PIXEL_UNPACK_BUFFER, _input_texture_array_size, NULL, GL_STREAM_DRAW);

		glTexImage2D(GL_TEXTURE_2D, 0, (GLint)format, InputBufferBuilderWidth, InputBufferBuilderHeight, 0, format, GL_UNSIGNED_BYTE, nullptr);

		prepare_composite_input_shader();
		prepare_rgb_output_shader();

		glGenVertexArrays(1, &output_vertex_array);
		glGenBuffers(1, &output_array_buffer);
		output_vertices_per_slice = 0;

		glBindBuffer(GL_ARRAY_BUFFER, output_array_buffer);

		glBufferData(GL_ARRAY_BUFFER, InputVertexBufferDataSize, NULL, GL_STREAM_DRAW);
		_output_buffer_data_pointer = 0;

		glBindVertexArray(output_vertex_array);
		prepare_output_vertex_array();

		// This should return either an actual framebuffer number, if this is a target with a framebuffer intended for output,
		// or 0 if no framebuffer is bound, in which case 0 is also what we want to supply to bind the implied framebuffer. So
		// it works either way.
		glGetIntegerv(GL_FRAMEBUFFER_BINDING, (GLint *)&defaultFramebuffer);

		// Create intermediate textures and bind to slots 0, 1 and 2
//		glActiveTexture(GL_TEXTURE0);
//		compositeTexture = std::unique_ptr<OpenGL::TextureTarget>(new OpenGL::TextureTarget(IntermediateBufferWidth, IntermediateBufferHeight));
//		glActiveTexture(GL_TEXTURE1);
//		filteredYTexture = std::unique_ptr<OpenGL::TextureTarget>(new OpenGL::TextureTarget(IntermediateBufferWidth, IntermediateBufferHeight));
//		glActiveTexture(GL_TEXTURE2);
//		filteredTexture = std::unique_ptr<OpenGL::TextureTarget>(new OpenGL::TextureTarget(IntermediateBufferWidth, IntermediateBufferHeight));
	}

	// lock down any further work on the current frame
	_output_mutex->lock();

	// release the mapping, giving up on trying to draw if data has been lost
	if(glUnmapBuffer(GL_ARRAY_BUFFER) == GL_FALSE)
	{
		for(int c = 0; c < NumberOfFields; c++)
			_run_builders[c]->reset();
	}
	glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);

	// upload more source pixel data if any; we'll always resubmit the last line submitted last
	// time as it may have had extra data appended to it
//	for(unsigned int buffer = 0; buffer < _buffer_builder->number_of_buffers; buffer++)
//	{
//		glActiveTexture(GL_TEXTURE0 + first_supplied_buffer_texture_unit + buffer);
		GLenum format = formatForDepth(_buffer_builder->bytes_per_pixel);
		if(_buffer_builder->_next_write_y_position < _buffer_builder->last_uploaded_line)
		{
			glTexSubImage2D(GL_TEXTURE_2D, 0,
								0, (GLint)_buffer_builder->last_uploaded_line,
								InputBufferBuilderWidth, (GLint)(InputBufferBuilderHeight - _buffer_builder->last_uploaded_line),
								format, GL_UNSIGNED_BYTE,
								(void *)(_buffer_builder->last_uploaded_line * InputBufferBuilderWidth * _buffer_builder->bytes_per_pixel));
			_buffer_builder->last_uploaded_line = 0;
		}

		if(_buffer_builder->_next_write_y_position > _buffer_builder->last_uploaded_line)
		{
			glTexSubImage2D(GL_TEXTURE_2D, 0,
								0, (GLint)_buffer_builder->last_uploaded_line,
								InputBufferBuilderWidth, (GLint)(1 + _buffer_builder->_next_write_y_position - _buffer_builder->last_uploaded_line),
								format, GL_UNSIGNED_BYTE,
								(void *)(_buffer_builder->last_uploaded_line * InputBufferBuilderWidth * _buffer_builder->bytes_per_pixel));
			_buffer_builder->last_uploaded_line = _buffer_builder->_next_write_y_position;
		}
//	}

	// check for anything to decode from composite
//	if(_composite_src_runs->number_of_vertices)
//	{
//		composite_input_shader_program->bind();
//		_composite_src_runs->reset();
//	}

//	_output_mutex->unlock();
//	return;

	// reinstate the output framebuffer
//	glBindTexture(GL_TEXTURE_2D, _openGL_state->textureName);
//	glGetIntegerv(GL_VIEWPORT, results);

	// switch to the output shader
	if(rgb_shader_program)
	{
		rgb_shader_program->bind();

		// update uniforms
		push_size_uniforms(output_width, output_height);

		// Ensure we're back on the output framebuffer
		glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebuffer);

		// clear the buffer
		glClear(GL_COLOR_BUFFER_BIT);

		// draw all sitting frames
		unsigned int run = (unsigned int)_run_write_pointer;
		GLint total_age = 0;
		for(int c = 0; c < NumberOfFields; c++)
		{
			// update the total age at the start of this set of runs
			total_age += _run_builders[run]->duration;

			if(_run_builders[run]->amount_of_data > 0)
			{
				// draw
				glUniform1f(timestampBaseUniform, (GLfloat)total_age);
				GLsizei count = (GLsizei)(_run_builders[run]->amount_of_data / InputVertexSize);
				GLsizei max_count = (GLsizei)((InputVertexBufferDataSize - _run_builders[run]->start) / InputVertexSize);
				if(count < max_count)
				{
					glDrawArrays(GL_TRIANGLE_STRIP, (GLint)(_run_builders[run]->start / InputVertexSize), count);
				}
				else
				{
					glDrawArrays(GL_TRIANGLE_STRIP, (GLint)(_run_builders[run]->start / InputVertexSize), max_count);
					glDrawArrays(GL_TRIANGLE_STRIP, 0, count - max_count);
				}
			}

			// advance back in time
			run = (run - 1 + NumberOfFields) % NumberOfFields;
		}
	}

	// drawing commands having been issued, reclaim the array buffer pointer
	_buffer_builder->move_to_new_line();
	_output_buffer_data = (uint8_t *)glMapBufferRange(GL_ARRAY_BUFFER, 0, InputVertexBufferDataSize, GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT);
	_input_texture_data = (uint8_t *)glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, _input_texture_array_size, GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT);
	_output_mutex->unlock();
}

void OpenGLOutputBuilder::set_openGL_context_will_change(bool should_delete_resources)
{
}

void OpenGLOutputBuilder::push_size_uniforms(unsigned int output_width, unsigned int output_height)
{
	if(windowSizeUniform >= 0)
	{
		glUniform2f(windowSizeUniform, output_width, output_height);
	}

	GLfloat outputAspectRatioMultiplier = ((float)output_width / (float)output_height) / (4.0f / 3.0f);

	Rect _aspect_ratio_corrected_bounds = _visible_area;

	GLfloat bonusWidth = (outputAspectRatioMultiplier - 1.0f) * _visible_area.size.width;
	_aspect_ratio_corrected_bounds.origin.x -= bonusWidth * 0.5f * _aspect_ratio_corrected_bounds.size.width;
	_aspect_ratio_corrected_bounds.size.width *= outputAspectRatioMultiplier;

	if(boundsOriginUniform >= 0)
		glUniform2f(boundsOriginUniform, (GLfloat)_aspect_ratio_corrected_bounds.origin.x, (GLfloat)_aspect_ratio_corrected_bounds.origin.y);

	if(boundsSizeUniform >= 0)
		glUniform2f(boundsSizeUniform, (GLfloat)_aspect_ratio_corrected_bounds.size.width, (GLfloat)_aspect_ratio_corrected_bounds.size.height);
}

void OpenGLOutputBuilder::set_composite_sampling_function(const char *shader)
{
	_composite_shader = strdup(shader);
}

void OpenGLOutputBuilder::set_rgb_sampling_function(const char *shader)
{
	_rgb_shader = strdup(shader);
}

#pragma mark - Input vertex shader (i.e. from source data to intermediate line layout)

char *OpenGLOutputBuilder::get_input_vertex_shader()
{
	return strdup(
		"#version 150\n"

		"in vec2 inputPosition;"
		"in vec2 outputPosition;"
		"in vec2 phaseAndAmplitude;"
		"in float phaseTime;"

		"uniform vec2 outputTextureSize;"
		"uniform vec2 inputTextureSize;"
		"uniform float phaseCyclesPerTick;"

		"out vec2 inputPositionVarying;"
		"out float phaseVarying;"

		"void main(void)"
		"{"
			"inputPositionVarying = vec2(inputPositionVarying.x / inputTextureSize.x, (inputPositionVarying.y + 0.5) / inputTextureSize.y);"
			"gl_Position = vec4(outputPosition.x * 2.0 / outputTextureSize - 1.0, outputPosition.y * 2.0 / outputTextureSize - 1.0, 0.0, 1.0);"
			"phaseVarying = (phaseCyclesPerTick * phaseTime + phaseAndAmplitude.x) * 2.0 * 3.141592654;"
		"}");
}

char *OpenGLOutputBuilder::get_input_fragment_shader()
{
	const char *composite_shader = _composite_shader;
	if(!composite_shader)
	{
		// TODO: synthesise an RGB -> (selected colour space) shader
	}

	return get_compound_shader(
		"#version 150\n"

		"in vec2 inputPositionVarying;"
		"in float phaseVarying;"

		"out vec4 fragColour;"

		"uniform sampler2D texID;"

		"\n%s\n"

		"void main(void)"
		"{"
			"fragColour = vec4(composite_sample(inputPositionVarying, phaseVarying), 0.0, 0.0, 1.0);"
		"}"
	, composite_shader);
}

#pragma mark - Intermediate vertex shaders (i.e. from intermediate line layout to intermediate line layout)

#pragma mark - Output vertex shader

char *OpenGLOutputBuilder::get_output_vertex_shader()
{
	// the main job of the vertex shader is just to map from an input area of [0,1]x[0,1], with the origin in the
	// top left to OpenGL's [-1,1]x[-1,1] with the origin in the lower left, and to convert input data coordinates
	// from integral to floating point.

	return strdup(
		"#version 150\n"

		"in vec2 position;"
		"in vec2 srcCoordinates;"
		"in float lateral;"
		"in float timestamp;"

		"uniform vec2 boundsOrigin;"
		"uniform vec2 boundsSize;"

		"out float lateralVarying;"
		"out vec2 shadowMaskCoordinates;"
		"out float alpha;"

		"uniform vec2 textureSize;"
		"uniform float timestampBase;"
		"uniform float ticksPerFrame;"
		"uniform vec2 positionConversion;"
		"uniform vec2 scanNormal;"
		"uniform vec3 filterCoefficients;"

		"const float shadowMaskMultiple = 600;"

		"out vec2 srcCoordinatesVarying;"

		"void main(void)"
		"{"
			"lateralVarying = lateral + 1.0707963267949;"

			"shadowMaskCoordinates = position * vec2(shadowMaskMultiple, shadowMaskMultiple * 0.85057471264368);"

			"srcCoordinatesVarying = vec2(srcCoordinates.x / textureSize.x, (srcCoordinates.y + 0.5) / textureSize.y);"
			"float age = (timestampBase - timestamp) / ticksPerFrame;"
			"vec3 alphas = vec3(10.0 * exp((-age - 0.66) * 2.0), 10.0 * exp(-(age - 0.33) * 2.0), 10.0 * exp(-age * 2.0));"
//			"alpha = min(10.0 * exp(-age * 2.0), 1.0);"
			"alpha = dot(alphas, filterCoefficients);"

			"vec2 floatingPosition = (position / positionConversion) + lateral*scanNormal;"
			"vec2 mappedPosition = (floatingPosition - boundsOrigin) / boundsSize;"
			"gl_Position = vec4(mappedPosition.x * 2.0 - 1.0, 1.0 - mappedPosition.y * 2.0, 0.0, 1.0);"
		"}");
}

#pragma mark - Output fragment shaders; RGB and from composite

char *OpenGLOutputBuilder::get_rgb_output_fragment_shader()
{
	return get_output_fragment_shader(_rgb_shader);
}

char *OpenGLOutputBuilder::get_composite_output_fragment_shader()
{
	return get_output_fragment_shader(
		"vec4 rgb_sample(vec2 coordinate)"
		"{"
			"return texture(texID, coordinate);"
		"}");
}

char *OpenGLOutputBuilder::get_output_fragment_shader(const char *sampling_function)
{
	return get_compound_shader(
		"#version 150\n"

		"in float lateralVarying;"
		"in float alpha;"
		"in vec2 shadowMaskCoordinates;"
		"in vec2 srcCoordinatesVarying;"

		"out vec4 fragColour;"

		"uniform sampler2D texID;"
		"uniform sampler2D shadowMaskTexID;"

		"\n%s\n"

		"void main(void)"
		"{"
			"fragColour = rgb_sample(srcCoordinatesVarying) * vec4(1.0, 1.0, 1.0, alpha * sin(lateralVarying));" //
		"}"
	, sampling_function);
}

#pragma mark - Shader utilities

char *OpenGLOutputBuilder::get_compound_shader(const char *base, const char *insert)
{
	if(!base || !insert) return nullptr;
	size_t totalLength = strlen(base) + strlen(insert) + 1;
	char *text = new char[totalLength];
	snprintf(text, totalLength, base, insert);
	return text;
}

#pragma mark - Program compilation

void OpenGLOutputBuilder::prepare_composite_input_shader()
{
	char *vertex_shader = get_input_vertex_shader();
	char *fragment_shader = get_input_fragment_shader();
	if(vertex_shader && fragment_shader)
	{
		composite_input_shader_program = std::unique_ptr<OpenGL::Shader>(new OpenGL::Shader(vertex_shader, fragment_shader));

		GLint texIDUniform				= composite_input_shader_program->get_uniform_location("texID");
		GLint inputTextureSizeUniform	= composite_input_shader_program->get_uniform_location("inputTextureSize");
		GLint outputTextureSizeUniform	= composite_input_shader_program->get_uniform_location("outputTextureSize");
		GLint phaseCyclesPerTickUniform	= composite_input_shader_program->get_uniform_location("phaseCyclesPerTick");

		glUniform1i(texIDUniform, first_supplied_buffer_texture_unit);
		glUniform2f(outputTextureSizeUniform, IntermediateBufferWidth, IntermediateBufferHeight);
		glUniform2f(inputTextureSizeUniform, InputBufferBuilderWidth, InputBufferBuilderHeight);
		glUniform1f(phaseCyclesPerTickUniform, (float)_colour_cycle_numerator / (float)(_colour_cycle_denominator * _cycles_per_line));
	}
	free(vertex_shader);
	free(fragment_shader);
}

/*void OpenGLOutputBuilder::prepare_output_shader(char *fragment_shader)
{
	char *vertex_shader = get_output_vertex_shader();
	if(vertex_shader && fragment_shader)
	{
		_openGL_state->rgb_shader_program = std::unique_ptr<OpenGL::Shader>(new OpenGL::Shader(vertex_shader, fragment_shader));

		_openGL_state->rgb_shader_program->bind();

		_openGL_state->windowSizeUniform			= _openGL_state->rgb_shader_program->get_uniform_location("windowSize");
		_openGL_state->boundsSizeUniform			= _openGL_state->rgb_shader_program->get_uniform_location("boundsSize");
		_openGL_state->boundsOriginUniform			= _openGL_state->rgb_shader_program->get_uniform_location("boundsOrigin");
		_openGL_state->timestampBaseUniform			= _openGL_state->rgb_shader_program->get_uniform_location("timestampBase");

		GLint texIDUniform				= _openGL_state->rgb_shader_program->get_uniform_location("texID");
		GLint shadowMaskTexIDUniform	= _openGL_state->rgb_shader_program->get_uniform_location("shadowMaskTexID");
		GLint textureSizeUniform		= _openGL_state->rgb_shader_program->get_uniform_location("textureSize");
		GLint ticksPerFrameUniform		= _openGL_state->rgb_shader_program->get_uniform_location("ticksPerFrame");
		GLint scanNormalUniform			= _openGL_state->rgb_shader_program->get_uniform_location("scanNormal");
		GLint positionConversionUniform	= _openGL_state->rgb_shader_program->get_uniform_location("positionConversion");

		glUniform1i(texIDUniform, first_supplied_buffer_texture_unit);
		glUniform1i(shadowMaskTexIDUniform, 1);
		glUniform2f(textureSizeUniform, CRTInputBufferBuilderWidth, CRTInputBufferBuilderHeight);
		glUniform1f(ticksPerFrameUniform, (GLfloat)(_cycles_per_line * _height_of_display));
		glUniform2f(positionConversionUniform, _horizontal_flywheel->get_scan_period(), _vertical_flywheel->get_scan_period() / (unsigned int)_vertical_flywheel_output_divider);

		float scan_angle = atan2f(1.0f / (float)_height_of_display, 1.0f);
		float scan_normal[] = { -sinf(scan_angle), cosf(scan_angle)};
		float multiplier = (float)_horizontal_flywheel->get_standard_period() / ((float)_height_of_display * (float)_horizontal_flywheel->get_scan_period());
		scan_normal[0] *= multiplier;
		scan_normal[1] *= multiplier;
		glUniform2f(scanNormalUniform, scan_normal[0], scan_normal[1]);
	}

	free(vertex_shader);
	free(fragment_shader);
}*/

void OpenGLOutputBuilder::prepare_rgb_output_shader()
{
	char *vertex_shader = get_output_vertex_shader();
	char *fragment_shader = get_rgb_output_fragment_shader();

	if(vertex_shader && fragment_shader)
	{
		rgb_shader_program = std::unique_ptr<OpenGL::Shader>(new OpenGL::Shader(vertex_shader, fragment_shader));

		rgb_shader_program->bind();

		windowSizeUniform			= rgb_shader_program->get_uniform_location("windowSize");
		boundsSizeUniform			= rgb_shader_program->get_uniform_location("boundsSize");
		boundsOriginUniform			= rgb_shader_program->get_uniform_location("boundsOrigin");
		timestampBaseUniform		= rgb_shader_program->get_uniform_location("timestampBase");

		GLint texIDUniform				= rgb_shader_program->get_uniform_location("texID");
		GLint shadowMaskTexIDUniform	= rgb_shader_program->get_uniform_location("shadowMaskTexID");
		GLint textureSizeUniform		= rgb_shader_program->get_uniform_location("textureSize");
		GLint ticksPerFrameUniform		= rgb_shader_program->get_uniform_location("ticksPerFrame");
		GLint scanNormalUniform			= rgb_shader_program->get_uniform_location("scanNormal");
		GLint positionConversionUniform	= rgb_shader_program->get_uniform_location("positionConversion");
		GLint filterCoefficients		= rgb_shader_program->get_uniform_location("filterCoefficients");

		glUniform1i(texIDUniform, first_supplied_buffer_texture_unit);
		glUniform1i(shadowMaskTexIDUniform, 1);
		glUniform2f(textureSizeUniform, InputBufferBuilderWidth, InputBufferBuilderHeight);
		glUniform1f(ticksPerFrameUniform, (GLfloat)(_cycles_per_line * _height_of_display));
		glUniform2f(positionConversionUniform, _horizontal_scan_period, _vertical_scan_period / (unsigned int)_vertical_period_divider);

		SignalProcessing::FIRFilter filter(3, 3 * 50, 0, 25, SignalProcessing::FIRFilter::DefaultAttenuation);
		float coefficients[3];
		filter.get_coefficients(coefficients);
		glUniform3fv(filterCoefficients, 1, coefficients);

		float scan_angle = atan2f(1.0f / (float)_height_of_display, 1.0f);
		float scan_normal[] = { -sinf(scan_angle), cosf(scan_angle)};
		float multiplier = (float)_cycles_per_line / ((float)_height_of_display * (float)_horizontal_scan_period);
		scan_normal[0] *= multiplier;
		scan_normal[1] *= multiplier;
		glUniform2f(scanNormalUniform, scan_normal[0], scan_normal[1]);
	}

	free(vertex_shader);
	free(fragment_shader);
}

void OpenGLOutputBuilder::prepare_output_vertex_array()
{
	if(rgb_shader_program)
	{
		GLint positionAttribute				= rgb_shader_program->get_attrib_location("position");
		GLint textureCoordinatesAttribute	= rgb_shader_program->get_attrib_location("srcCoordinates");
		GLint lateralAttribute				= rgb_shader_program->get_attrib_location("lateral");
		GLint timestampAttribute			= rgb_shader_program->get_attrib_location("timestamp");

		glEnableVertexAttribArray((GLuint)positionAttribute);
		glEnableVertexAttribArray((GLuint)textureCoordinatesAttribute);
		glEnableVertexAttribArray((GLuint)lateralAttribute);
		glEnableVertexAttribArray((GLuint)timestampAttribute);

		const GLsizei vertexStride = OutputVertexSize;
		glVertexAttribPointer((GLuint)positionAttribute,			2, GL_UNSIGNED_SHORT,	GL_FALSE,	vertexStride, (void *)OutputVertexOffsetOfPosition);
		glVertexAttribPointer((GLuint)textureCoordinatesAttribute,	2, GL_UNSIGNED_SHORT,	GL_FALSE,	vertexStride, (void *)OutputVertexOffsetOfTexCoord);
		glVertexAttribPointer((GLuint)timestampAttribute,			4, GL_UNSIGNED_INT,		GL_FALSE,	vertexStride, (void *)OutputVertexOffsetOfTimestamp);
		glVertexAttribPointer((GLuint)lateralAttribute,				1, GL_UNSIGNED_BYTE,	GL_FALSE,	vertexStride, (void *)OutputVertexOffsetOfLateral);
	}
}

#pragma mark - Configuration

void OpenGLOutputBuilder::set_output_device(OutputDevice output_device)
{
	if (_output_device != output_device)
	{
		_output_device = output_device;

//		for(int builder = 0; builder < NumberOfFields; builder++)
//		{
//			_run_builders[builder]->reset();
//		}
//		_composite_src_runs->reset();
		_composite_src_output_y = 0;
	}
}


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
//}

