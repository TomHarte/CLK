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
#include "Shaders/OutputShader.hpp"

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

static int getCircularRanges(GLsizei start, GLsizei end, GLsizei buffer_length, GLsizei granularity, GLsizei *ranges)
{
	GLsizei startOffset = start%granularity;
	if(startOffset)
	{
		start -= startOffset;
	}

	GLsizei length = end - start;
	if(!length) return 0;
	if(length > buffer_length)
	{
		ranges[0] = 0;
		ranges[1] = buffer_length;
		return 1;
	}
	else
	{
		ranges[0] = start % buffer_length;
		if(ranges[0]+length < buffer_length)
		{
			ranges[1] = length;
			return 1;
		}
		else
		{
			ranges[1] = buffer_length - ranges[0];
			ranges[2] = 0;
			ranges[3] = length - ranges[1];
			return 2;
		}
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
	_output_mutex(new std::mutex),
	_visible_area(Rect(0, 0, 1, 1)),
	_composite_src_output_y(0),
	_cleared_composite_output_y(0),
	_composite_shader(nullptr),
	_rgb_shader(nullptr),
	_output_buffer_data(nullptr),
	_source_buffer_data(nullptr),
	_input_texture_data(nullptr),
	_output_buffer_data_pointer(0),
	_drawn_output_buffer_data_pointer(0),
	_source_buffer_data_pointer(0),
	_drawn_source_buffer_data_pointer(0)
{
	_buffer_builder = std::unique_ptr<CRTInputBufferBuilder>(new CRTInputBufferBuilder(buffer_depth));

	glBlendFunc(GL_SRC_ALPHA, GL_CONSTANT_COLOR);
	glBlendColor(0.4f, 0.4f, 0.4f, 0.5f);

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

		set_timing_uniforms();
		set_colour_space_uniforms();

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
	glUnmapBuffer(GL_ARRAY_BUFFER);
	glBindBuffer(GL_ARRAY_BUFFER, source_array_buffer);
	glUnmapBuffer(GL_ARRAY_BUFFER);
	glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);

	// upload more source pixel data if any; we'll always resubmit the last line submitted last
	// time as it may have had extra data appended to it
	if(_buffer_builder->_write_y_position < _buffer_builder->last_uploaded_line)
	{
		glTexSubImage2D(	GL_TEXTURE_2D, 0,
							0, (GLint)_buffer_builder->last_uploaded_line,
							InputBufferBuilderWidth, (GLint)(InputBufferBuilderHeight - _buffer_builder->last_uploaded_line),
							formatForDepth(_buffer_builder->bytes_per_pixel), GL_UNSIGNED_BYTE,
							(void *)(_buffer_builder->last_uploaded_line * InputBufferBuilderWidth * _buffer_builder->bytes_per_pixel));
		_buffer_builder->last_uploaded_line = 0;
	}

	if(_buffer_builder->_write_y_position > _buffer_builder->last_uploaded_line)
	{
		glTexSubImage2D(	GL_TEXTURE_2D, 0,
							0, (GLint)_buffer_builder->last_uploaded_line,
							InputBufferBuilderWidth, (GLint)(1 + _buffer_builder->_next_write_y_position - _buffer_builder->last_uploaded_line),
							formatForDepth(_buffer_builder->bytes_per_pixel), GL_UNSIGNED_BYTE,
							(void *)(_buffer_builder->last_uploaded_line * InputBufferBuilderWidth * _buffer_builder->bytes_per_pixel));
		_buffer_builder->last_uploaded_line = _buffer_builder->_next_write_y_position;
	}

	// for television, update intermediate buffers and then draw; for a monitor, just draw
	if(_output_device == Television || !rgb_shader_program)
	{
		// decide how much to draw
		if(_drawn_source_buffer_data_pointer != _source_buffer_data_pointer)
		{
			// determine how many lines are newly reclaimed; they'll need to be cleared
			GLsizei clearing_zones[4], drawing_zones[4];
			int number_of_clearing_zones = getCircularRanges(_cleared_composite_output_y+1, _composite_src_output_y+1, IntermediateBufferHeight, 1, clearing_zones);
			int number_of_drawing_zones = getCircularRanges(_drawn_source_buffer_data_pointer, _source_buffer_data_pointer, SourceVertexBufferDataSize, 2*SourceVertexSize, drawing_zones);

			_composite_src_output_y %= IntermediateBufferHeight;
			_cleared_composite_output_y = _composite_src_output_y;
			_source_buffer_data_pointer %= SourceVertexBufferDataSize;
			_drawn_source_buffer_data_pointer = _source_buffer_data_pointer;

			// all drawing will be from the source vertex array and without blending
			glBindVertexArray(source_vertex_array);
			glDisable(GL_BLEND);

			OpenGL::TextureTarget *targets[] = {
				compositeTexture.get(),
				filteredYTexture.get(),
				filteredTexture.get()
			};
			OpenGL::Shader *shaders[] = {
				composite_input_shader_program.get(),
				composite_y_filter_shader_program.get(),
				composite_chrominance_filter_shader_program.get()
			};
			float clear_colours[][3] = {
				{0.0, 0.0, 0.0},
				{0.0, 0.5, 0.5},
				{0.0, 0.0, 0.0}
			};
			for(int stage = 0; stage < 3; stage++)
			{
				// switch to the initial texture
				targets[stage]->bind_framebuffer();
				shaders[stage]->bind();

				// clear as desired
				if(number_of_clearing_zones)
				{
					glEnable(GL_SCISSOR_TEST);
					glClearColor(clear_colours[stage][0], clear_colours[stage][1], clear_colours[stage][2], 1.0);
					for(int c = 0; c < number_of_clearing_zones; c++)
					{
						glScissor(0, clearing_zones[c*2], IntermediateBufferWidth, clearing_zones[c*2 + 1]);
						glClear(GL_COLOR_BUFFER_BIT);
					}
					glDisable(GL_SCISSOR_TEST);
				}

				// draw as desired
				for(int c = 0; c < number_of_drawing_zones; c++)
				{
					glDrawArrays(GL_LINES, drawing_zones[c*2] / SourceVertexSize, drawing_zones[c*2 + 1] / SourceVertexSize);
				}
			}

			// switch back to screen output
			glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebuffer);
			glViewport(0, 0, (GLsizei)output_width, (GLsizei)output_height);
			glClearColor(0.0, 0.0, 0.0, 1.0);
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

void OpenGLOutputBuilder::perform_output_stage(unsigned int output_width, unsigned int output_height, OpenGL::OutputShader *const shader)
{
	if(shader)
	{
		// clear the buffer
//		glClear(GL_COLOR_BUFFER_BIT);

		// draw all pending lines
		GLsizei drawing_zones[4];
		int number_of_drawing_zones = getCircularRanges(_drawn_output_buffer_data_pointer, _output_buffer_data_pointer, OutputVertexBufferDataSize, 6*OutputVertexSize, drawing_zones);

		_output_buffer_data_pointer %= SourceVertexBufferDataSize;
		_output_buffer_data_pointer -= (_output_buffer_data_pointer%(6*OutputVertexSize));
		_drawn_output_buffer_data_pointer = _output_buffer_data_pointer;

		if(number_of_drawing_zones > 0)
		{
			glEnable(GL_BLEND);

			// Ensure we're back on the output framebuffer, drawing from the output array buffer
			glBindVertexArray(output_vertex_array);

			// update uniforms (implicitly binding the shader)
			shader->set_output_size(output_width, output_height, _visible_area);

			// draw
			for(int c = 0; c < number_of_drawing_zones; c++)
			{
				glDrawArrays(GL_TRIANGLE_STRIP, drawing_zones[c*2] / OutputVertexSize, drawing_zones[c*2 + 1] / OutputVertexSize);
			}
		}
	}
}

void OpenGLOutputBuilder::set_openGL_context_will_change(bool should_delete_resources)
{
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

char *OpenGLOutputBuilder::get_input_vertex_shader(const char *input_position, const char *header)
{
	char *result;
	asprintf(&result,
		"#version 150\n"

		"in vec2 inputPosition;"
		"in vec2 outputPosition;"
		"in vec3 phaseAmplitudeAndOffset;"
		"in float phaseTime;"

		"uniform float phaseCyclesPerTick;"
		"uniform ivec2 outputTextureSize;"
		"uniform float extension;"

		"\n%s\n"

		"out vec2 inputPositionVarying;"
		"out vec2 iInputPositionVarying;"
		"out float phaseVarying;"
		"out float amplitudeVarying;"
		"out vec2 inputPositionsVarying[11];"

		"void main(void)"
		"{"
			"vec2 extensionVector = vec2(extension, 0.0) * 2.0 * (phaseAmplitudeAndOffset.z - 0.5);"
			"vec2 extendedInputPosition = %s + extensionVector;"
			"vec2 extendedOutputPosition = outputPosition + extensionVector;"

			"vec2 textureSize = vec2(textureSize(texID, 0));"
			"iInputPositionVarying = extendedInputPosition;"
			"inputPositionVarying = (extendedInputPosition + vec2(0.0, 0.5)) / textureSize;"

			"textureSize = textureSize * vec2(1.0);"
			"inputPositionsVarying[0] = inputPositionVarying - (vec2(10.0, 0.0) / textureSize);"
			"inputPositionsVarying[1] = inputPositionVarying - (vec2(8.0, 0.0) / textureSize);"
			"inputPositionsVarying[2] = inputPositionVarying - (vec2(6.0, 0.0) / textureSize);"
			"inputPositionsVarying[3] = inputPositionVarying - (vec2(4.0, 0.0) / textureSize);"
			"inputPositionsVarying[4] = inputPositionVarying - (vec2(2.0, 0.0) / textureSize);"

			"inputPositionsVarying[5] = inputPositionVarying;"

			"inputPositionsVarying[6] = inputPositionVarying + (vec2(2.0, 0.0) / textureSize);"
			"inputPositionsVarying[7] = inputPositionVarying + (vec2(4.0, 0.0) / textureSize);"
			"inputPositionsVarying[8] = inputPositionVarying + (vec2(6.0, 0.0) / textureSize);"
			"inputPositionsVarying[9] = inputPositionVarying + (vec2(8.0, 0.0) / textureSize);"
			"inputPositionsVarying[10] = inputPositionVarying + (vec2(10.0, 0.0) / textureSize);"

			"phaseVarying = (phaseCyclesPerTick * (extendedOutputPosition.x - phaseTime) + phaseAmplitudeAndOffset.x) * 2.0 * 3.141592654;"
			"amplitudeVarying = 0.33;" // phaseAmplitudeAndOffset.y

			"vec2 eyePosition = 2.0*(extendedOutputPosition / outputTextureSize) - vec2(1.0) + vec2(0.5)/textureSize;"
			"gl_Position = vec4(eyePosition, 0.0, 1.0);"
		"}", header, input_position);
	return result;
}

char *OpenGLOutputBuilder::get_input_fragment_shader()
{
	char *composite_shader = _composite_shader;
	if(!composite_shader)
	{
		asprintf(&composite_shader,
			"%s\n"
			"uniform mat3 rgbToLumaChroma;"
			"float composite_sample(usampler2D texID, vec2 coordinate, vec2 iCoordinate, float phase, float amplitude)"
			"{"
				"vec3 rgbColour = clamp(rgb_sample(texID, coordinate, iCoordinate), vec3(0.0), vec3(1.0));"
				"vec3 lumaChromaColour = rgbToLumaChroma * rgbColour;"
				"vec2 quadrature = vec2(cos(phase), -sin(phase)) * amplitude;"
				"return dot(lumaChromaColour, vec3(1.0 - amplitude, quadrature));"
			"}",
			_rgb_shader);
		// TODO: use YIQ if this is NTSC
	}

	char *result;
	asprintf(&result,
		"#version 150\n"

		"in vec2 inputPositionVarying;"
		"in vec2 iInputPositionVarying;"
		"in float phaseVarying;"
		"in float amplitudeVarying;"

		"out vec4 fragColour;"

		"uniform usampler2D texID;"

		"\n%s\n"

		"void main(void)"
		"{"
			"fragColour = vec4(composite_sample(texID, inputPositionVarying, iInputPositionVarying, phaseVarying, amplitudeVarying));"
		"}"
	, composite_shader);

	if(!_composite_shader) free(composite_shader);

	return result;
}

char *OpenGLOutputBuilder::get_y_filter_fragment_shader()
{
	return strdup(
		"#version 150\n"

		"in float phaseVarying;"
		"in float amplitudeVarying;"

		"in vec2 inputPositionsVarying[11];"
		"uniform vec4 weights[3];"

		"out vec3 fragColour;"

		"uniform sampler2D texID;"

		"void main(void)"
		"{"
			"vec4 samples[3] = vec4[]("
				"vec4("
					"texture(texID, inputPositionsVarying[0]).r,"
					"texture(texID, inputPositionsVarying[1]).r,"
					"texture(texID, inputPositionsVarying[2]).r,"
					"texture(texID, inputPositionsVarying[3]).r"
				"),"
				"vec4("
					"texture(texID, inputPositionsVarying[4]).r,"
					"texture(texID, inputPositionsVarying[5]).r,"
					"texture(texID, inputPositionsVarying[6]).r,"
					"texture(texID, inputPositionsVarying[7]).r"
				"),"
				"vec4("
					"texture(texID, inputPositionsVarying[8]).r,"
					"texture(texID, inputPositionsVarying[9]).r,"
					"texture(texID, inputPositionsVarying[10]).r,"
					"0.0"
				")"
			");"

			"float luminance = "
				"dot(vec3("
					"dot(samples[0], weights[0]),"
					"dot(samples[1], weights[1]),"
					"dot(samples[2], weights[2])"
				"), vec3(1.0)) / (1.0 - amplitudeVarying);"

			"float chrominance = 0.5 * (samples[1].y - luminance) / amplitudeVarying;"
			"vec2 quadrature = vec2(cos(phaseVarying), -sin(phaseVarying));"

			"fragColour = vec3(luminance, vec2(0.5) + (chrominance * quadrature));"
		"}");
}

char *OpenGLOutputBuilder::get_chrominance_filter_fragment_shader()
{
	return strdup(
		"#version 150\n"

		"in float phaseVarying;"
		"in float amplitudeVarying;"

		"in vec2 inputPositionsVarying[11];"
		"uniform vec4 weights[3];"

		"out vec3 fragColour;"

		"uniform sampler2D texID;"
		"uniform mat3 lumaChromaToRGB;"

		"void main(void)"
		"{"
			"vec3 centreSample = texture(texID, inputPositionsVarying[5]).rgb;"
			"vec2 samples[] = vec2[]("
				"texture(texID, inputPositionsVarying[0]).gb - vec2(0.5),"
				"texture(texID, inputPositionsVarying[1]).gb - vec2(0.5),"
				"texture(texID, inputPositionsVarying[2]).gb - vec2(0.5),"
				"texture(texID, inputPositionsVarying[3]).gb - vec2(0.5),"
				"texture(texID, inputPositionsVarying[4]).gb - vec2(0.5),"
				"centreSample.gb - vec2(0.5),"
				"texture(texID, inputPositionsVarying[6]).gb - vec2(0.5),"
				"texture(texID, inputPositionsVarying[7]).gb - vec2(0.5),"
				"texture(texID, inputPositionsVarying[8]).gb - vec2(0.5),"
				"texture(texID, inputPositionsVarying[9]).gb - vec2(0.5),"
				"texture(texID, inputPositionsVarying[10]).gb - vec2(0.5)"
			");"

			"vec4 channel1[] = vec4[]("
				"vec4(samples[0].r, samples[1].r, samples[2].r, samples[3].r),"
				"vec4(samples[4].r, samples[5].r, samples[6].r, samples[7].r),"
				"vec4(samples[8].r, samples[9].r, samples[10].r, 0.0)"
			");"
			"vec4 channel2[] = vec4[]("
				"vec4(samples[0].g, samples[1].g, samples[2].g, samples[3].g),"
				"vec4(samples[4].g, samples[5].g, samples[6].g, samples[7].g),"
				"vec4(samples[8].g, samples[9].g, samples[10].g, 0.0)"
			");"

			"vec3 lumaChromaColour = vec3(centreSample.r,"
				"dot(vec3("
					"dot(channel1[0], weights[0]),"
					"dot(channel1[1], weights[1]),"
					"dot(channel1[2], weights[2])"
				"), vec3(1.0)) + 0.5,"
				"dot(vec3("
					"dot(channel2[0], weights[0]),"
					"dot(channel2[1], weights[1]),"
					"dot(channel2[2], weights[2])"
				"), vec3(1.0)) + 0.5"
			");"

			"vec3 lumaChromaColourInRange = (lumaChromaColour - vec3(0.0, 0.5, 0.5)) * vec3(1.0, 2.0, 2.0);"
			"fragColour = lumaChromaToRGB * lumaChromaColourInRange;"
		"}");
}


#pragma mark - Intermediate vertex shaders (i.e. from intermediate line layout to intermediate line layout)

#pragma mark - Program compilation

std::unique_ptr<OpenGL::Shader> OpenGLOutputBuilder::prepare_intermediate_shader(const char *input_position, const char *header, char *fragment_shader, GLenum texture_unit, bool extends)
{
	std::unique_ptr<OpenGL::Shader> shader;
	char *vertex_shader = get_input_vertex_shader(input_position, header);
	if(vertex_shader && fragment_shader)
	{
		OpenGL::Shader::AttributeBinding bindings[] =
		{
			{"inputPosition", 0},
			{"outputPosition", 1},
			{"phaseAmplitudeAndOffset", 2},
			{"phaseTime", 3},
			{nullptr}
		};
		shader = std::unique_ptr<OpenGL::Shader>(new OpenGL::Shader(vertex_shader, fragment_shader, bindings));

		GLint texIDUniform				= shader->get_uniform_location("texID");
		GLint outputTextureSizeUniform	= shader->get_uniform_location("outputTextureSize");

		shader->bind();
		glUniform1i(texIDUniform, (GLint)(texture_unit - GL_TEXTURE0));
		glUniform2i(outputTextureSizeUniform, IntermediateBufferWidth, IntermediateBufferHeight);
	}
	free(vertex_shader);
	free(fragment_shader);

	return shader;
}

void OpenGLOutputBuilder::prepare_composite_input_shader()
{
	composite_input_shader_program = prepare_intermediate_shader("inputPosition", "uniform usampler2D texID;", get_input_fragment_shader(), source_data_texture_unit, false);
	composite_y_filter_shader_program = prepare_intermediate_shader("outputPosition", "uniform sampler2D texID;", get_y_filter_fragment_shader(), composite_texture_unit, true);
	composite_chrominance_filter_shader_program = prepare_intermediate_shader("outputPosition", "uniform sampler2D texID;", get_chrominance_filter_fragment_shader(), filtered_y_texture_unit, true);
}

void OpenGLOutputBuilder::prepare_source_vertex_array()
{
	if(composite_input_shader_program)
	{
		GLint inputPositionAttribute			= composite_input_shader_program->get_attrib_location("inputPosition");
		GLint outputPositionAttribute			= composite_input_shader_program->get_attrib_location("outputPosition");
		GLint phaseAmplitudeAndOffsetAttribute	= composite_input_shader_program->get_attrib_location("phaseAmplitudeAndOffset");
		GLint phaseTimeAttribute				= composite_input_shader_program->get_attrib_location("phaseTime");

		glBindVertexArray(source_vertex_array);

		glEnableVertexAttribArray((GLuint)inputPositionAttribute);
		glEnableVertexAttribArray((GLuint)outputPositionAttribute);
		glEnableVertexAttribArray((GLuint)phaseAmplitudeAndOffsetAttribute);
		glEnableVertexAttribArray((GLuint)phaseTimeAttribute);

		const GLsizei vertexStride = SourceVertexSize;
		glBindBuffer(GL_ARRAY_BUFFER, source_array_buffer);
		glVertexAttribPointer((GLuint)inputPositionAttribute,			2, GL_UNSIGNED_SHORT,	GL_FALSE,	vertexStride, (void *)SourceVertexOffsetOfInputPosition);
		glVertexAttribPointer((GLuint)outputPositionAttribute,			2, GL_UNSIGNED_SHORT,	GL_FALSE,	vertexStride, (void *)SourceVertexOffsetOfOutputPosition);
		glVertexAttribPointer((GLuint)phaseAmplitudeAndOffsetAttribute,	3, GL_UNSIGNED_BYTE,	GL_TRUE,	vertexStride, (void *)SourceVertexOffsetOfPhaseAmplitudeAndOffset);
		glVertexAttribPointer((GLuint)phaseTimeAttribute,				2, GL_UNSIGNED_SHORT,	GL_FALSE,	vertexStride, (void *)SourceVertexOffsetOfPhaseTime);
	}
}

void OpenGLOutputBuilder::prepare_rgb_output_shader()
{
	const char *rgb_shader = _rgb_shader;
	if(!_rgb_shader)
	{
		rgb_shader =
			"vec3 rgb_sample(usampler2D sampler, vec2 coordinate, vec2 icoordinate)"
			"{"
				"return texture(sampler, coordinate).rgb / vec3(255.0);"
			"}";
	}

	rgb_shader_program = OpenGL::OutputShader::make_shader(rgb_shader, "rgb_sample(texID, srcCoordinatesVarying, iSrcCoordinatesVarying)", true);
	rgb_shader_program->set_source_texture_unit(source_data_texture_unit);
}

void OpenGLOutputBuilder::prepare_composite_output_shader()
{
	composite_output_shader_program = OpenGL::OutputShader::make_shader("", "texture(texID, srcCoordinatesVarying).rgb", false);
	composite_output_shader_program->set_source_texture_unit(filtered_texture_unit);
}

void OpenGLOutputBuilder::prepare_output_vertex_array()
{
	if(rgb_shader_program)
	{
		GLint positionAttribute				= rgb_shader_program->get_attrib_location("position");
		GLint textureCoordinatesAttribute	= rgb_shader_program->get_attrib_location("srcCoordinates");
		GLint lateralAttribute				= rgb_shader_program->get_attrib_location("lateral");

		glBindVertexArray(output_vertex_array);

		glEnableVertexAttribArray((GLuint)positionAttribute);
		glEnableVertexAttribArray((GLuint)textureCoordinatesAttribute);
		glEnableVertexAttribArray((GLuint)lateralAttribute);

		const GLsizei vertexStride = OutputVertexSize;
		glBindBuffer(GL_ARRAY_BUFFER, output_array_buffer);
		glVertexAttribPointer((GLuint)positionAttribute,			2, GL_UNSIGNED_SHORT,	GL_FALSE,	vertexStride, (void *)OutputVertexOffsetOfPosition);
		glVertexAttribPointer((GLuint)textureCoordinatesAttribute,	2, GL_UNSIGNED_SHORT,	GL_FALSE,	vertexStride, (void *)OutputVertexOffsetOfTexCoord);
		glVertexAttribPointer((GLuint)lateralAttribute,				1, GL_UNSIGNED_BYTE,	GL_FALSE,	vertexStride, (void *)OutputVertexOffsetOfLateral);
	}
}

#pragma mark - Public Configuration

void OpenGLOutputBuilder::set_output_device(OutputDevice output_device)
{
	if(_output_device != output_device)
	{
		_output_device = output_device;
		_composite_src_output_y = 0;
	}
}

void OpenGLOutputBuilder::set_timing(unsigned int cycles_per_line, unsigned int height_of_display, unsigned int horizontal_scan_period, unsigned int vertical_scan_period, unsigned int vertical_period_divider)
{
	_cycles_per_line = cycles_per_line;
	_height_of_display = height_of_display;
	_horizontal_scan_period = horizontal_scan_period;
	_vertical_scan_period = vertical_scan_period;
	_vertical_period_divider = vertical_period_divider;

	set_timing_uniforms();
}

#pragma mark - Internal Configuration

void OpenGLOutputBuilder::set_colour_space_uniforms()
{
	_output_mutex->lock();
	GLfloat rgbToYUV[] = {0.299f, -0.14713f, 0.615f, 0.587f, -0.28886f, -0.51499f, 0.114f, 0.436f, -0.10001f};
	GLfloat yuvToRGB[] = {1.0f, 1.0f, 1.0f, 0.0f, -0.39465f, 2.03211f, 1.13983f, -0.58060f, 0.0f};

	GLfloat rgbToYIQ[] = {0.299f, 0.596f, 0.211f, 0.587f, -0.274f, -0.523f, 0.114f, -0.322f, 0.312f};
	GLfloat yiqToRGB[] = {1.0f, 1.0f, 1.0f, 0.956f, -0.272f, -1.106f, 0.621f, -0.647f, 1.703f};

	GLfloat *fromRGB, *toRGB;

	switch(_colour_space)
	{
		case ColourSpace::YIQ:
			fromRGB = rgbToYIQ;
			toRGB = yiqToRGB;
		break;

		case ColourSpace::YUV:
			fromRGB = rgbToYUV;
			toRGB = yuvToRGB;
		break;
	}

	if(composite_input_shader_program)
	{
		composite_input_shader_program->bind();
		GLint uniform = composite_input_shader_program->get_uniform_location("rgbToLumaChroma");
		if(uniform >= 0)
		{
			glUniformMatrix3fv(uniform, 1, GL_FALSE, fromRGB);
		}
	}

	if(composite_chrominance_filter_shader_program)
	{
		composite_chrominance_filter_shader_program->bind();
		GLint uniform = composite_chrominance_filter_shader_program->get_uniform_location("lumaChromaToRGB");
		if(uniform >= 0)
		{
			glUniformMatrix3fv(uniform, 1, GL_FALSE, toRGB);
		}
	}
	_output_mutex->unlock();
}

void OpenGLOutputBuilder::set_timing_uniforms()
{
	_output_mutex->lock();
	OpenGL::Shader *intermediate_shaders[] = {
		composite_input_shader_program.get(),
		composite_y_filter_shader_program.get(),
		composite_chrominance_filter_shader_program.get()
	};
	bool extends = false;
	for(int c = 0; c < 3; c++)
	{
		if(intermediate_shaders[c])
		{
			intermediate_shaders[c]->bind();
			GLint phaseCyclesPerTickUniform	= intermediate_shaders[c]->get_uniform_location("phaseCyclesPerTick");
			GLint extensionUniform			= intermediate_shaders[c]->get_uniform_location("extension");

			float phaseCyclesPerTick = (float)_colour_cycle_numerator / (float)(_colour_cycle_denominator * _cycles_per_line);
			glUniform1f(phaseCyclesPerTickUniform, phaseCyclesPerTick);
			glUniform1f(extensionUniform, extends ? ceilf(1.0f / phaseCyclesPerTick) : 0.0f);
		}
		extends = true;
	}

	OpenGL::OutputShader *output_shaders[] = {
		rgb_shader_program.get(),
		composite_output_shader_program.get()
	};
	for(int c = 0; c < 2; c++)
	{
		if(output_shaders[c])
		{
			output_shaders[c]->set_timing(_height_of_display, _cycles_per_line, _horizontal_scan_period, _vertical_scan_period, _vertical_period_divider);
		}
	}

	float colour_subcarrier_frequency = (float)_colour_cycle_numerator / (float)_colour_cycle_denominator;
	GLint weightsUniform;
	float weights[12];

	if(composite_y_filter_shader_program)
	{
		SignalProcessing::FIRFilter luminance_filter(11, _cycles_per_line * 0.5f, 0.0f, colour_subcarrier_frequency * 0.5f, SignalProcessing::FIRFilter::DefaultAttenuation);
		composite_y_filter_shader_program->bind();
		weightsUniform = composite_y_filter_shader_program->get_uniform_location("weights");
		luminance_filter.get_coefficients(weights);
		glUniform4fv(weightsUniform, 3, weights);
	}

	if(composite_chrominance_filter_shader_program)
	{
		SignalProcessing::FIRFilter chrominance_filter(11, _cycles_per_line * 0.5f, 0.0f, colour_subcarrier_frequency * 0.5f, SignalProcessing::FIRFilter::DefaultAttenuation);
		composite_chrominance_filter_shader_program->bind();
		weightsUniform = composite_chrominance_filter_shader_program->get_uniform_location("weights");
		chrominance_filter.get_coefficients(weights);
		glUniform4fv(weightsUniform, 3, weights);
	}
	_output_mutex->unlock();
}
