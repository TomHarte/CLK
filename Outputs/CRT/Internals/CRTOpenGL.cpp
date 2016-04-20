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

static const GLint internalFormatForDepth(size_t depth)
{
	switch(depth)
	{
		default: return GL_FALSE;
		case 1: return GL_R8UI;
		case 2: return GL_RG8UI;
		case 3: return GL_RGB8UI;
		case 4: return GL_RGBA8UI;
	}
}

static const GLenum formatForDepth(size_t depth)
{
	switch(depth)
	{
		default: return GL_FALSE;
		case 1: return GL_RED_INTEGER;
		case 2: return GL_RG_INTEGER;
		case 3: return GL_RGB_INTEGER;
		case 4: return GL_RGBA_INTEGER;
	}
}

using namespace Outputs::CRT;

namespace {
	static const GLenum composite_texture_unit = GL_TEXTURE0;
	static const GLenum filtered_y_texture_unit = GL_TEXTURE1;
	static const GLenum filtered_texture_unit = GL_TEXTURE2;
	static const GLenum source_data_texture_unit = GL_TEXTURE3;
}

OpenGLOutputBuilder::OpenGLOutputBuilder(unsigned int buffer_depth) :
	_run_write_pointer(0),
	_output_mutex(new std::mutex),
	_visible_area(Rect(0, 0, 1, 1)),
	_composite_src_output_y(0),
	_composite_shader(nullptr),
	_rgb_shader(nullptr),
	_output_buffer_data(nullptr),
	_source_buffer_data(nullptr),
	_input_texture_data(nullptr),
	_output_buffer_data_pointer(0),
	_source_buffer_data_pointer(0)
{
	_run_builders = new CRTRunBuilder *[NumberOfFields];
	for(int builder = 0; builder < NumberOfFields; builder++)
	{
		_run_builders[builder] = new CRTRunBuilder();
	}
	_buffer_builder = std::unique_ptr<CRTInputBufferBuilder>(new CRTInputBufferBuilder(buffer_depth));

	glBlendFunc(GL_SRC_ALPHA, GL_ONE);

	// Create intermediate textures and bind to slots 0, 1 and 2
	glActiveTexture(composite_texture_unit);
	compositeTexture = std::unique_ptr<OpenGL::TextureTarget>(new OpenGL::TextureTarget(IntermediateBufferWidth, IntermediateBufferHeight));
	compositeTexture->bind_texture();
	glActiveTexture(filtered_y_texture_unit);
	filteredYTexture = std::unique_ptr<OpenGL::TextureTarget>(new OpenGL::TextureTarget(IntermediateBufferWidth, IntermediateBufferHeight));
	filteredYTexture->bind_texture();
	glActiveTexture(filtered_texture_unit);
	filteredTexture = std::unique_ptr<OpenGL::TextureTarget>(new OpenGL::TextureTarget(IntermediateBufferWidth, IntermediateBufferHeight));
	filteredTexture->bind_texture();

	// create the surce texture
	glGenTextures(1, &textureName);
	glActiveTexture(source_data_texture_unit);
	glBindTexture(GL_TEXTURE_2D, textureName);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, internalFormatForDepth(_buffer_builder->bytes_per_pixel), InputBufferBuilderWidth, InputBufferBuilderHeight, 0, formatForDepth(_buffer_builder->bytes_per_pixel), GL_UNSIGNED_BYTE, nullptr);

	// create a pixel unpack buffer
	glGenBuffers(1, &_input_texture_array);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, _input_texture_array);
	_input_texture_array_size = (GLsizeiptr)(InputBufferBuilderWidth * InputBufferBuilderHeight * _buffer_builder->bytes_per_pixel);
	glBufferData(GL_PIXEL_UNPACK_BUFFER, _input_texture_array_size, NULL, GL_STREAM_DRAW);

	// map the buffer for clients
	_input_texture_data = (uint8_t *)glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, _input_texture_array_size, GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT);

	// create the output vertex array
	glGenVertexArrays(1, &output_vertex_array);

	// create a buffer for output vertex attributes
	glGenBuffers(1, &output_array_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, output_array_buffer);
	glBufferData(GL_ARRAY_BUFFER, OutputVertexBufferDataSize, NULL, GL_STREAM_DRAW);

	// map that buffer too, for any CRT activity that may occur before the first draw
	_output_buffer_data = (uint8_t *)glMapBufferRange(GL_ARRAY_BUFFER, 0, OutputVertexBufferDataSize, GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT);

	// create the source vertex array
	glGenVertexArrays(1, &source_vertex_array);

	// create a buffer for source vertex attributes
	glGenBuffers(1, &source_array_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, source_array_buffer);
	glBufferData(GL_ARRAY_BUFFER, SourceVertexBufferDataSize, NULL, GL_STREAM_DRAW);

	// map that buffer too, for any CRT activity that may occur before the first draw
	_source_buffer_data = (uint8_t *)glMapBufferRange(GL_ARRAY_BUFFER, 0, SourceVertexBufferDataSize, GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT);

	// map back the default framebuffer
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
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
	glDeleteBuffers(1, &source_array_buffer);
	glDeleteVertexArrays(1, &output_vertex_array);

	free(_composite_shader);
	free(_rgb_shader);
}

void OpenGLOutputBuilder::draw_frame(unsigned int output_width, unsigned int output_height, bool only_if_dirty)
{
	// establish essentials
	if(!composite_input_shader_program && !rgb_shader_program)
	{
		prepare_composite_input_shader();
		prepare_source_vertex_array();

		prepare_composite_output_shader();
		prepare_rgb_output_shader();
		prepare_output_vertex_array();

		// This should return either an actual framebuffer number, if this is a target with a framebuffer intended for output,
		// or 0 if no framebuffer is bound, in which case 0 is also what we want to supply to bind the implied framebuffer. So
		// it works either way.
		glGetIntegerv(GL_FRAMEBUFFER_BINDING, (GLint *)&defaultFramebuffer);

		// TODO: is this sustainable, cross-platform? If so, why store it at all?
		defaultFramebuffer = 0;
	}

	// lock down any further work on the current frame
	_output_mutex->lock();

	// release the mapping, giving up on trying to draw if data has been lost
	glBindBuffer(GL_ARRAY_BUFFER, output_array_buffer);
	if(glUnmapBuffer(GL_ARRAY_BUFFER) == GL_FALSE)
	{
		for(int c = 0; c < NumberOfFields; c++)
			_run_builders[c]->reset();
	}
	glBindBuffer(GL_ARRAY_BUFFER, source_array_buffer);
	glUnmapBuffer(GL_ARRAY_BUFFER);
	glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);

	// upload more source pixel data if any; we'll always resubmit the last line submitted last
	// time as it may have had extra data appended to it
	if(_buffer_builder->_next_write_y_position < _buffer_builder->last_uploaded_line)
	{
		glTexSubImage2D(	GL_TEXTURE_2D, 0,
							0, (GLint)_buffer_builder->last_uploaded_line,
							InputBufferBuilderWidth, (GLint)(InputBufferBuilderHeight - _buffer_builder->last_uploaded_line),
							formatForDepth(_buffer_builder->bytes_per_pixel), GL_UNSIGNED_BYTE,
							(void *)(_buffer_builder->last_uploaded_line * InputBufferBuilderWidth * _buffer_builder->bytes_per_pixel));
		_buffer_builder->last_uploaded_line = 0;
	}

	if(_buffer_builder->_next_write_y_position > _buffer_builder->last_uploaded_line)
	{
		glTexSubImage2D(	GL_TEXTURE_2D, 0,
							0, (GLint)_buffer_builder->last_uploaded_line,
							InputBufferBuilderWidth, (GLint)(1 + _buffer_builder->_next_write_y_position - _buffer_builder->last_uploaded_line),
							formatForDepth(_buffer_builder->bytes_per_pixel), GL_UNSIGNED_BYTE,
							(void *)(_buffer_builder->last_uploaded_line * InputBufferBuilderWidth * _buffer_builder->bytes_per_pixel));
		_buffer_builder->last_uploaded_line = _buffer_builder->_next_write_y_position;
	}

	// for television, update intermediate buffers and then draw; for a monitor, just draw
	if(_output_device == Television)
	{
		// decide how much to draw
		if(_drawn_source_buffer_data_pointer != _source_buffer_data_pointer)
		{
			composite_input_shader_program->bind();

			compositeTexture->bind_framebuffer();
			glBindVertexArray(source_vertex_array);

			glDisable(GL_BLEND);

			size_t new_data_size = _source_buffer_data_pointer - _drawn_source_buffer_data_pointer;
			size_t new_data_start = _drawn_source_buffer_data_pointer;
			_source_buffer_data_pointer %= SourceVertexBufferDataSize;
			_drawn_source_buffer_data_pointer = _source_buffer_data_pointer;

			if(new_data_size >= SourceVertexBufferDataSize)
			{
				new_data_size = SourceVertexBufferDataSize;
				new_data_start = 0;
			}

			size_t first_data_length = std::min(SourceVertexBufferDataSize - new_data_start, new_data_size);
			glDrawArrays(GL_LINES, (GLint)(new_data_start / SourceVertexSize), (GLsizei)(first_data_length / SourceVertexSize));
			if(new_data_size > first_data_length)
			{
				glDrawArrays(GL_LINES, 0, (GLsizei)((new_data_size - first_data_length) / SourceVertexSize));
			}

			glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebuffer);
			glViewport(0, 0, (GLsizei)output_width, (GLsizei)output_height);
		}

		// transfer to screen
		perform_output_stage(output_width, output_height, composite_output_shader_program.get());
	}
	else
		perform_output_stage(output_width, output_height, rgb_shader_program.get());

	// drawing commands having been issued, reclaim the array buffer pointer
	glBindBuffer(GL_ARRAY_BUFFER, output_array_buffer);
	_output_buffer_data = (uint8_t *)glMapBufferRange(GL_ARRAY_BUFFER, 0, OutputVertexBufferDataSize, GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT);

	glBindBuffer(GL_ARRAY_BUFFER, source_array_buffer);
	_source_buffer_data = (uint8_t *)glMapBufferRange(GL_ARRAY_BUFFER, 0, SourceVertexBufferDataSize, GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT);

	_input_texture_data = (uint8_t *)glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, _input_texture_array_size, GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT);

	_output_mutex->unlock();
}

void OpenGLOutputBuilder::perform_output_stage(unsigned int output_width, unsigned int output_height, OpenGL::Shader *const shader)
{
	if(shader)
	{
		// clear the buffer
		glClear(GL_COLOR_BUFFER_BIT);

		// draw all sitting frames
		unsigned int run = (unsigned int)_run_write_pointer;
		GLint total_age = 0;
		float timestampBases[4];
		size_t start = 0, count = 0;
		for(int c = 0; c < NumberOfFields; c++)
		{
			total_age += _run_builders[run]->duration;
			timestampBases[run] = (float)total_age;
			count += _run_builders[run]->amount_of_data;
			start = _run_builders[run]->start;
			run = (run - 1 + NumberOfFields) % NumberOfFields;
		}

		if(count > 0)
		{
			glEnable(GL_BLEND);

			// Ensure we're back on the output framebuffer, drawing from the output array buffer
			glBindVertexArray(output_vertex_array);
			shader->bind();

			// update uniforms
			push_size_uniforms(output_width, output_height);

			// draw
			glUniform4fv(timestampBaseUniform, 1, timestampBases);

			GLsizei primitive_count = (GLsizei)(count / OutputVertexSize);
			GLsizei max_count = (GLsizei)((OutputVertexBufferDataSize - start) / OutputVertexSize);
			if(primitive_count < max_count)
			{
				glDrawArrays(GL_TRIANGLE_STRIP, (GLint)(start / OutputVertexSize), primitive_count);
			}
			else
			{
				glDrawArrays(GL_TRIANGLE_STRIP, (GLint)(start / OutputVertexSize), max_count);
				glDrawArrays(GL_TRIANGLE_STRIP, 0, primitive_count - max_count);
			}
		}
	}
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
		"in vec3 phaseAmplitudeAndAlpha;"
		"in float phaseTime;"

		"uniform float phaseCyclesPerTick;"
		"uniform usampler2D texID;"
		"uniform ivec2 outputTextureSize;"

		"out vec2 inputPositionVarying;"
		"out vec2 iInputPositionVarying;"
		"out float phaseVarying;"
		"out float alphaVarying;"

		"void main(void)"
		"{"
			"ivec2 textureSize = textureSize(texID, 0);"
			"iInputPositionVarying = inputPosition;"
			"inputPositionVarying = (inputPosition + vec2(0.0, 0.5)) / vec2(textureSize);"

			"phaseVarying = (phaseCyclesPerTick * phaseTime + phaseAmplitudeAndAlpha.x) * 2.0 * 3.141592654;"
			"alphaVarying = phaseAmplitudeAndAlpha.z;"

			"vec2 eyePosition = 2.0*(outputPosition / outputTextureSize) - vec2(1.0) + vec2(0.5)/textureSize;"
			"gl_Position = vec4(eyePosition, 0.0, 1.0);"
		"}");
}

char *OpenGLOutputBuilder::get_input_fragment_shader()
{
	const char *composite_shader = _composite_shader;
	if(!composite_shader)
	{
		composite_shader = _rgb_shader;
		// TODO: synthesise an RGB -> (selected colour space) shader
	}

	char *result;
	asprintf(&result,
		"#version 150\n"

		"in vec2 inputPositionVarying;"
		"in vec2 iInputPositionVarying;"
		"in float phaseVarying;"
		"in float alphaVarying;"

		"out vec4 fragColour;"

		"uniform usampler2D texID;"

		"\n%s\n"

		"void main(void)"
		"{"
			"fragColour = vec4(rgb_sample(texID, inputPositionVarying, iInputPositionVarying) * alphaVarying, 1.0);" // composite
		"}"
	, composite_shader);
	return result;
}

#pragma mark - Intermediate vertex shaders (i.e. from intermediate line layout to intermediate line layout)

#pragma mark - Output vertex shader

char *OpenGLOutputBuilder::get_output_vertex_shader(const char *header)
{
	// the main job of the vertex shader is just to map from an input area of [0,1]x[0,1], with the origin in the
	// top left to OpenGL's [-1,1]x[-1,1] with the origin in the lower left, and to convert input data coordinates
	// from integral to floating point.

	char *result;
	asprintf(&result,
		"#version 150\n"

		"in vec2 position;"
		"in vec2 srcCoordinates;"
		"in vec2 lateralAndTimestampBaseOffset;"
		"in float timestamp;"

		"uniform vec2 boundsOrigin;"
		"uniform vec2 boundsSize;"

		"out float lateralVarying;"
//		"out vec2 shadowMaskCoordinates;"
		"out float alpha;"

		"uniform vec4 timestampBase;"
		"uniform float ticksPerFrame;"
		"uniform vec2 positionConversion;"
		"uniform vec2 scanNormal;"

		"\n%s\n"
//		"uniform sampler2D shadowMaskTexID;"

//		"const float shadowMaskMultiple = 600;"

		"out vec2 srcCoordinatesVarying;"
		"out vec2 iSrcCoordinatesVarying;"

		"void main(void)"
		"{"
			"lateralVarying = lateralAndTimestampBaseOffset.x + 1.0707963267949;"

//			"shadowMaskCoordinates = position * vec2(shadowMaskMultiple, shadowMaskMultiple * 0.85057471264368);"

			"ivec2 textureSize = textureSize(texID, 0);"
			"iSrcCoordinatesVarying = srcCoordinates;"
			"srcCoordinatesVarying = vec2(srcCoordinates.x / textureSize.x, (srcCoordinates.y + 0.5) / textureSize.y);"
			"float age = (timestampBase[int(lateralAndTimestampBaseOffset.y)] - timestamp) / ticksPerFrame;"
			"alpha = exp(-age) + 0.2;"

			"vec2 floatingPosition = (position / positionConversion) + lateralAndTimestampBaseOffset.x * scanNormal;"
			"vec2 mappedPosition = (floatingPosition - boundsOrigin) / boundsSize;"
			"gl_Position = vec4(mappedPosition.x * 2.0 - 1.0, 1.0 - mappedPosition.y * 2.0, 0.0, 1.0);"
		"}", header);
	return result;
}

char *OpenGLOutputBuilder::get_rgb_output_vertex_shader()
{
	return get_output_vertex_shader("uniform usampler2D texID;");
}

char *OpenGLOutputBuilder::get_composite_output_vertex_shader()
{
	return get_output_vertex_shader("uniform sampler2D texID;");
}

#pragma mark - Output fragment shaders; RGB and from composite

char *OpenGLOutputBuilder::get_rgb_output_fragment_shader()
{
	return get_output_fragment_shader(_rgb_shader, "uniform usampler2D texID;", "rgb_sample(texID, srcCoordinatesVarying, iSrcCoordinatesVarying)");
}

char *OpenGLOutputBuilder::get_composite_output_fragment_shader()
{
	return get_output_fragment_shader("", "uniform sampler2D texID;", "texture(texID, srcCoordinatesVarying).rgb");
}

char *OpenGLOutputBuilder::get_output_fragment_shader(const char *sampling_function, const char *header, const char *fragColour_function)
{
	char *result;
	asprintf(&result,
		"#version 150\n"

		"in float lateralVarying;"
		"in float alpha;"
//		"in vec2 shadowMaskCoordinates;"
		"in vec2 srcCoordinatesVarying;"
		"in vec2 iSrcCoordinatesVarying;"

		"out vec4 fragColour;"

//		"uniform sampler2D shadowMaskTexID;",
		"%s\n"
		"%s\n"
		"void main(void)"
		"{"
			"fragColour = vec4(%s, clamp(alpha, 0.0, 1.0)*sin(lateralVarying));"
		"}",
	header, sampling_function, fragColour_function);

	return result;
}

#pragma mark - Program compilation

void OpenGLOutputBuilder::prepare_composite_input_shader()
{
	char *vertex_shader = get_input_vertex_shader();
	char *fragment_shader = get_input_fragment_shader();
	if(vertex_shader && fragment_shader)
	{
		composite_input_shader_program = std::unique_ptr<OpenGL::Shader>(new OpenGL::Shader(vertex_shader, fragment_shader, nullptr));

		GLint texIDUniform				= composite_input_shader_program->get_uniform_location("texID");
		GLint phaseCyclesPerTickUniform	= composite_input_shader_program->get_uniform_location("phaseCyclesPerTick");
		GLint outputTextureSizeUniform	= composite_input_shader_program->get_uniform_location("outputTextureSize");

		composite_input_shader_program->bind();
		glUniform1i(texIDUniform, source_data_texture_unit - GL_TEXTURE0);
		glUniform1f(phaseCyclesPerTickUniform, (float)_colour_cycle_numerator / (float)(_colour_cycle_denominator * _cycles_per_line));
		glUniform2i(outputTextureSizeUniform, IntermediateBufferWidth, IntermediateBufferHeight);
	}
	free(vertex_shader);
	free(fragment_shader);
}

void OpenGLOutputBuilder::prepare_source_vertex_array()
{
	if(composite_input_shader_program)
	{
		GLint inputPositionAttribute			= composite_input_shader_program->get_attrib_location("inputPosition");
		GLint outputPositionAttribute			= composite_input_shader_program->get_attrib_location("outputPosition");
		GLint phaseAmplitudeAndAlphaAttribute	= composite_input_shader_program->get_attrib_location("phaseAmplitudeAndAlpha");
		GLint phaseTimeAttribute				= composite_input_shader_program->get_attrib_location("phaseTime");

		glBindVertexArray(source_vertex_array);

		glEnableVertexAttribArray((GLuint)inputPositionAttribute);
		glEnableVertexAttribArray((GLuint)outputPositionAttribute);
		glEnableVertexAttribArray((GLuint)phaseAmplitudeAndAlphaAttribute);
		glEnableVertexAttribArray((GLuint)phaseTimeAttribute);

		const GLsizei vertexStride = SourceVertexSize;
		glBindBuffer(GL_ARRAY_BUFFER, source_array_buffer);
		glVertexAttribPointer((GLuint)inputPositionAttribute,			2, GL_UNSIGNED_SHORT,	GL_FALSE,	vertexStride, (void *)SourceVertexOffsetOfInputPosition);
		glVertexAttribPointer((GLuint)outputPositionAttribute,			2, GL_UNSIGNED_SHORT,	GL_FALSE,	vertexStride, (void *)SourceVertexOffsetOfOutputPosition);
		glVertexAttribPointer((GLuint)phaseAmplitudeAndAlphaAttribute,	3, GL_UNSIGNED_BYTE,	GL_TRUE,	vertexStride, (void *)SourceVertexOffsetOfPhaseAmplitudeAndAlpha);
		glVertexAttribPointer((GLuint)phaseTimeAttribute,				2, GL_UNSIGNED_SHORT,	GL_FALSE,	vertexStride, (void *)SourceVertexOffsetOfPhaseTime);
	}
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

std::unique_ptr<OpenGL::Shader> OpenGLOutputBuilder::prepare_output_shader(char *vertex_shader, char *fragment_shader, GLint source_texture_unit)
{
	std::unique_ptr<OpenGL::Shader> shader_program;

	if(vertex_shader && fragment_shader)
	{
		OpenGL::Shader::AttributeBinding bindings[] =
		{
			{"position", 0},
			{"srcCoordinates", 1},
			{"lateralAndTimestampBaseOffset", 2},
			{"timestamp", 3},
			{nullptr}
		};
		shader_program = std::unique_ptr<OpenGL::Shader>(new OpenGL::Shader(vertex_shader, fragment_shader, bindings));
		shader_program->bind();

		windowSizeUniform			= shader_program->get_uniform_location("windowSize");
		boundsSizeUniform			= shader_program->get_uniform_location("boundsSize");
		boundsOriginUniform			= shader_program->get_uniform_location("boundsOrigin");
		timestampBaseUniform		= shader_program->get_uniform_location("timestampBase");

		GLint texIDUniform				= shader_program->get_uniform_location("texID");
		GLint ticksPerFrameUniform		= shader_program->get_uniform_location("ticksPerFrame");
		GLint scanNormalUniform			= shader_program->get_uniform_location("scanNormal");
		GLint positionConversionUniform	= shader_program->get_uniform_location("positionConversion");

		glUniform1i(texIDUniform, source_texture_unit - GL_TEXTURE0);
		glUniform1f(ticksPerFrameUniform, (GLfloat)(_cycles_per_line * _height_of_display));
		glUniform2f(positionConversionUniform, _horizontal_scan_period, _vertical_scan_period / (unsigned int)_vertical_period_divider);

		float scan_angle = atan2f(1.0f / (float)_height_of_display, 1.0f);
		float scan_normal[] = { -sinf(scan_angle), cosf(scan_angle)};
		float multiplier = (float)_cycles_per_line / ((float)_height_of_display * (float)_horizontal_scan_period);
		scan_normal[0] *= multiplier;
		scan_normal[1] *= multiplier;
		glUniform2f(scanNormalUniform, scan_normal[0], scan_normal[1]);
	}

	free(vertex_shader);
	free(fragment_shader);

	return shader_program;
}

void OpenGLOutputBuilder::prepare_rgb_output_shader()
{
	rgb_shader_program = prepare_output_shader(get_rgb_output_vertex_shader(), get_rgb_output_fragment_shader(), source_data_texture_unit);
}

void OpenGLOutputBuilder::prepare_composite_output_shader()
{
	composite_output_shader_program = prepare_output_shader(get_composite_output_vertex_shader(), get_composite_output_fragment_shader(), composite_texture_unit);
}

void OpenGLOutputBuilder::prepare_output_vertex_array()
{
	if(rgb_shader_program)
	{
		GLint positionAttribute				= rgb_shader_program->get_attrib_location("position");
		GLint textureCoordinatesAttribute	= rgb_shader_program->get_attrib_location("srcCoordinates");
		GLint lateralAttribute				= rgb_shader_program->get_attrib_location("lateralAndTimestampBaseOffset");
		GLint timestampAttribute			= rgb_shader_program->get_attrib_location("timestamp");

		glBindVertexArray(output_vertex_array);

		glEnableVertexAttribArray((GLuint)positionAttribute);
		glEnableVertexAttribArray((GLuint)textureCoordinatesAttribute);
		glEnableVertexAttribArray((GLuint)lateralAttribute);
		glEnableVertexAttribArray((GLuint)timestampAttribute);

		const GLsizei vertexStride = OutputVertexSize;
		glBindBuffer(GL_ARRAY_BUFFER, output_array_buffer);
		glVertexAttribPointer((GLuint)positionAttribute,			2, GL_UNSIGNED_SHORT,	GL_FALSE,	vertexStride, (void *)OutputVertexOffsetOfPosition);
		glVertexAttribPointer((GLuint)textureCoordinatesAttribute,	2, GL_UNSIGNED_SHORT,	GL_FALSE,	vertexStride, (void *)OutputVertexOffsetOfTexCoord);
		glVertexAttribPointer((GLuint)timestampAttribute,			4, GL_UNSIGNED_INT,		GL_FALSE,	vertexStride, (void *)OutputVertexOffsetOfTimestamp);
		glVertexAttribPointer((GLuint)lateralAttribute,				2, GL_UNSIGNED_BYTE,	GL_FALSE,	vertexStride, (void *)OutputVertexOffsetOfLateral);
	}
}

#pragma mark - Configuration

void OpenGLOutputBuilder::set_output_device(OutputDevice output_device)
{
	if(_output_device != output_device)
	{
		_output_device = output_device;

		for(int builder = 0; builder < NumberOfFields; builder++)
		{
			_run_builders[builder]->reset();
		}

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

