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
	start -= start%granularity;
	end -= end%granularity;

	GLsizei length = end - start;
	if(!length) return 0;
	if(length >= buffer_length)
	{
		ranges[0] = 0;
		ranges[1] = buffer_length;
		return 1;
	}
	else
	{
		ranges[0] = start % buffer_length;
		if(ranges[0]+length <= buffer_length)
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
	static const GLenum composite_texture_unit			= GL_TEXTURE0;
	static const GLenum separated_texture_unit			= GL_TEXTURE1;
	static const GLenum filtered_y_texture_unit			= GL_TEXTURE2;
	static const GLenum filtered_texture_unit			= GL_TEXTURE3;
	static const GLenum source_data_texture_unit		= GL_TEXTURE4;
	static const GLenum pixel_accumulation_texture_unit	= GL_TEXTURE5;
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
	_output_buffer_data_pointer(0),
	_drawn_output_buffer_data_pointer(0),
	_source_buffer_data_pointer(0),
	_drawn_source_buffer_data_pointer(0),
	_last_output_width(0),
	_last_output_height(0)
{
	_buffer_builder = std::unique_ptr<CRTInputBufferBuilder>(new CRTInputBufferBuilder(buffer_depth));

	glBlendFunc(GL_SRC_ALPHA, GL_CONSTANT_COLOR);
	glBlendColor(0.6f, 0.6f, 0.6f, 1.0f);

	// Create intermediate textures and bind to slots 0, 1 and 2
	compositeTexture	= std::unique_ptr<OpenGL::TextureTarget>(new OpenGL::TextureTarget(IntermediateBufferWidth, IntermediateBufferHeight, composite_texture_unit));
	separatedTexture	= std::unique_ptr<OpenGL::TextureTarget>(new OpenGL::TextureTarget(IntermediateBufferWidth, IntermediateBufferHeight, separated_texture_unit));
	filteredYTexture	= std::unique_ptr<OpenGL::TextureTarget>(new OpenGL::TextureTarget(IntermediateBufferWidth, IntermediateBufferHeight, filtered_y_texture_unit));
	filteredTexture		= std::unique_ptr<OpenGL::TextureTarget>(new OpenGL::TextureTarget(IntermediateBufferWidth, IntermediateBufferHeight, filtered_texture_unit));

	// create the surce texture
	glGenTextures(1, &textureName);
	glActiveTexture(source_data_texture_unit);
	glBindTexture(GL_TEXTURE_2D, textureName);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, internalFormatForDepth(_buffer_builder->get_bytes_per_pixel()), InputBufferBuilderWidth, InputBufferBuilderHeight, 0, formatForDepth(_buffer_builder->get_bytes_per_pixel()), GL_UNSIGNED_BYTE, nullptr);

	// create the output vertex array
	glGenVertexArrays(1, &output_vertex_array);

	// create a buffer for output vertex attributes
	glGenBuffers(1, &output_array_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, output_array_buffer);
	glBufferData(GL_ARRAY_BUFFER, OutputVertexBufferDataSize, NULL, GL_STREAM_DRAW);

	// map that buffer too, for any CRT activity that may occur before the first draw
	_output_buffer_data = (uint8_t *)glMapBufferRange(GL_ARRAY_BUFFER, 0, OutputVertexBufferDataSize, GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);

	// create the source vertex array
	glGenVertexArrays(1, &source_vertex_array);

	// create a buffer for source vertex attributes
	glGenBuffers(1, &source_array_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, source_array_buffer);
	glBufferData(GL_ARRAY_BUFFER, SourceVertexBufferDataSize, NULL, GL_STREAM_DRAW);

	// map that buffer too, for any CRT activity that may occur before the first draw
	_source_buffer_data = (uint8_t *)glMapBufferRange(GL_ARRAY_BUFFER, 0, SourceVertexBufferDataSize, GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);
}

OpenGLOutputBuilder::~OpenGLOutputBuilder()
{
	glUnmapBuffer(GL_ARRAY_BUFFER);
	glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
	glDeleteTextures(1, &textureName);
	glDeleteBuffers(1, &output_array_buffer);
	glDeleteBuffers(1, &source_array_buffer);
	glDeleteVertexArrays(1, &output_vertex_array);

	free(_composite_shader);
	free(_rgb_shader);
}

void OpenGLOutputBuilder::draw_frame(unsigned int output_width, unsigned int output_height, bool only_if_dirty)
{
	// lock down any further work on the current frame
	_output_mutex->lock();

	// establish essentials
	if(!output_shader_program)
	{
		prepare_composite_input_shaders();
		prepare_rgb_input_shaders();
		prepare_source_vertex_array();

		prepare_output_shader();
		prepare_output_vertex_array();

		set_timing_uniforms();
		set_colour_space_uniforms();

		// This should return either an actual framebuffer number, if this is a target with a framebuffer intended for output,
		// or 0 if no framebuffer is bound, in which case 0 is also what we want to supply to bind the implied framebuffer. So
		// it works either way.
//		glGetIntegerv(GL_FRAMEBUFFER_BINDING, (GLint *)&defaultFramebuffer);

		// TODO: is this sustainable, cross-platform? If so, why store it at all?
//		defaultFramebuffer = 0;
	}

	// determine how many lines are newly reclaimed; they'll need to be cleared
	GLsizei clearing_zones[4], source_drawing_zones[4];
	GLsizei output_drawing_zones[4];
	int number_of_clearing_zones		= getCircularRanges(_cleared_composite_output_y, _composite_src_output_y, IntermediateBufferHeight, 1, clearing_zones);
	int number_of_source_drawing_zones	= getCircularRanges(_drawn_source_buffer_data_pointer, _source_buffer_data_pointer, SourceVertexBufferDataSize, 2*SourceVertexSize, source_drawing_zones);
	int number_of_output_drawing_zones	= getCircularRanges(_drawn_output_buffer_data_pointer, _output_buffer_data_pointer, OutputVertexBufferDataSize, 6*OutputVertexSize, output_drawing_zones);

//	for(int c = 0; c < number_of_output_drawing_zones; c++)
//	{
//		printf("\n(%d->%d)\n", output_drawing_zones[c*2], output_drawing_zones[c*2] + output_drawing_zones[c*2 + 1]);
//	}

//	if(number_of_output_drawing_zones)
//		printf("\n\n");

	uint16_t completed_texture_y = _buffer_builder->get_and_finalise_current_line();

	_composite_src_output_y %= IntermediateBufferHeight;
	_source_buffer_data_pointer %= SourceVertexBufferDataSize;
	_output_buffer_data_pointer %= OutputVertexBufferDataSize;

	_cleared_composite_output_y = _composite_src_output_y;
	_drawn_source_buffer_data_pointer = _source_buffer_data_pointer;
	_drawn_output_buffer_data_pointer = _output_buffer_data_pointer;

	// release the mapping, giving up on trying to draw if data has been lost
	glBindBuffer(GL_ARRAY_BUFFER, output_array_buffer);
	for(int c = 0; c < number_of_output_drawing_zones; c++)
	{
		glFlushMappedBufferRange(GL_ARRAY_BUFFER, output_drawing_zones[c*2], output_drawing_zones[c*2 + 1]);
	}
	glUnmapBuffer(GL_ARRAY_BUFFER);

	// bind and flush the source array buffer
	glBindBuffer(GL_ARRAY_BUFFER, source_array_buffer);
	for(int c = 0; c < number_of_source_drawing_zones; c++)
	{
		glFlushMappedBufferRange(GL_ARRAY_BUFFER, source_drawing_zones[c*2], source_drawing_zones[c*2 + 1]);
	}
	glUnmapBuffer(GL_ARRAY_BUFFER);

	// make sure there's a target to draw to
	if(!framebuffer || framebuffer->get_height() != output_height || framebuffer->get_width() != output_width)
	{
		std::unique_ptr<OpenGL::TextureTarget> new_framebuffer = std::unique_ptr<OpenGL::TextureTarget>(new OpenGL::TextureTarget((GLsizei)output_width, (GLsizei)output_height, pixel_accumulation_texture_unit));
		if(framebuffer)
		{
			new_framebuffer->bind_framebuffer();
			glClear(GL_COLOR_BUFFER_BIT);

			glActiveTexture(pixel_accumulation_texture_unit);
			framebuffer->bind_texture();
			framebuffer->draw((float)output_width / (float)output_height);

			new_framebuffer->bind_texture();
		}
		framebuffer = std::move(new_framebuffer);
	}

	// upload new source pixels
	if(completed_texture_y)
	{
		glActiveTexture(source_data_texture_unit);
		glTexSubImage2D(	GL_TEXTURE_2D, 0,
							0, 0,
							InputBufferBuilderWidth, completed_texture_y,
							formatForDepth(_buffer_builder->get_bytes_per_pixel()), GL_UNSIGNED_BYTE,
							_buffer_builder->get_image_pointer());
	}

	struct RenderStage {
		OpenGL::TextureTarget *const target;
		OpenGL::Shader *const shader;
		float clear_colour[3];
	};

	RenderStage composite_render_stages[] =
	{
		{compositeTexture.get(),	composite_input_shader_program.get(),				{0.0, 0.0, 0.0}},
		{separatedTexture.get(),	composite_separation_filter_program.get(),			{0.0, 0.5, 0.5}},
		{filteredYTexture.get(),	composite_y_filter_shader_program.get(),			{0.0, 0.5, 0.5}},
		{filteredTexture.get(),		composite_chrominance_filter_shader_program.get(),	{0.0, 0.0, 0.0}},
		{nullptr}
	};

	RenderStage rgb_render_stages[] =
	{
		{compositeTexture.get(),	rgb_input_shader_program.get(),		{0.0, 0.0, 0.0}},
		{filteredTexture.get(),		rgb_filter_shader_program.get(),	{0.0, 0.0, 0.0}},
		{nullptr}
	};

	RenderStage *active_pipeline = (_output_device == Television || !rgb_input_shader_program) ? composite_render_stages : rgb_render_stages;

	// for television, update intermediate buffers and then draw; for a monitor, just draw
	if(number_of_source_drawing_zones)
	{
		// all drawing will be from the source vertex array and without blending
		glBindVertexArray(source_vertex_array);
		glDisable(GL_BLEND);

		while(active_pipeline->target)
		{
			// switch to the initial texture
			active_pipeline->target->bind_framebuffer();
			active_pipeline->shader->bind();

			// clear as desired
			if(number_of_clearing_zones)
			{
				glEnable(GL_SCISSOR_TEST);
				glClearColor(active_pipeline->clear_colour[0], active_pipeline->clear_colour[1], active_pipeline->clear_colour[2], 1.0);
				for(int c = 0; c < number_of_clearing_zones; c++)
				{
					glScissor(0, clearing_zones[c*2], IntermediateBufferWidth, clearing_zones[c*2 + 1]);
					glClear(GL_COLOR_BUFFER_BIT);
				}
				glDisable(GL_SCISSOR_TEST);
			}

			// draw as desired
			for(int c = 0; c < number_of_source_drawing_zones; c++)
			{
				glDrawArrays(GL_LINES, source_drawing_zones[c*2] / SourceVertexSize, source_drawing_zones[c*2 + 1] / SourceVertexSize);
			}

			active_pipeline++;
		}
	}

	// transfer to framebuffer
	framebuffer->bind_framebuffer();


	if(number_of_output_drawing_zones)
	{
//	glClearColor(0.5, 0.5, 0.5, 1.0);
//	glClear(GL_COLOR_BUFFER_BIT);
		glEnable(GL_BLEND);

		// Ensure we're back on the output framebuffer, drawing from the output array buffer
		glBindVertexArray(output_vertex_array);

		// update uniforms (implicitly binding the shader)
		if(_last_output_width != output_width || _last_output_height != output_height)
		{
			output_shader_program->set_output_size(output_width, output_height, _visible_area);
			_last_output_width = output_width;
			_last_output_height = output_height;
		}
		output_shader_program->bind();

		// draw
		for(int c = 0; c < number_of_output_drawing_zones; c++)
		{
			glDrawArrays(GL_TRIANGLE_STRIP, output_drawing_zones[c*2] / OutputVertexSize, output_drawing_zones[c*2 + 1] / OutputVertexSize);
		}
//			glDrawArrays(GL_TRIANGLE_STRIP, 0, OutputVertexBufferDataSize / OutputVertexSize);
	}

	// copy framebuffer to the intended place
	glDisable(GL_BLEND);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, (GLsizei)output_width, (GLsizei)output_height);
	glClear(GL_COLOR_BUFFER_BIT);

	framebuffer->draw((float)output_width / (float)output_height);
//	filteredYTexture->draw((float)output_width / (float)output_height);

	// drawing commands having been issued, reclaim the array buffer pointer
	glBindBuffer(GL_ARRAY_BUFFER, output_array_buffer);
	_output_buffer_data = (uint8_t *)glMapBufferRange(GL_ARRAY_BUFFER, 0, OutputVertexBufferDataSize, GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);

	glBindBuffer(GL_ARRAY_BUFFER, source_array_buffer);
	_source_buffer_data = (uint8_t *)glMapBufferRange(GL_ARRAY_BUFFER, 0, SourceVertexBufferDataSize, GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);

	_output_mutex->unlock();
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

#pragma mark - Program compilation

void OpenGLOutputBuilder::prepare_composite_input_shaders()
{
	composite_input_shader_program = OpenGL::IntermediateShader::make_source_conversion_shader(_composite_shader, _rgb_shader);
	composite_input_shader_program->set_source_texture_unit(source_data_texture_unit);
	composite_input_shader_program->set_output_size(IntermediateBufferWidth, IntermediateBufferHeight);

	composite_separation_filter_program = OpenGL::IntermediateShader::make_chroma_luma_separation_shader();
	composite_separation_filter_program->set_source_texture_unit(composite_texture_unit);
	composite_separation_filter_program->set_output_size(IntermediateBufferWidth, IntermediateBufferHeight);

	composite_y_filter_shader_program = OpenGL::IntermediateShader::make_luma_filter_shader();
	composite_y_filter_shader_program->set_source_texture_unit(separated_texture_unit);
	composite_y_filter_shader_program->set_output_size(IntermediateBufferWidth, IntermediateBufferHeight);

	composite_chrominance_filter_shader_program = OpenGL::IntermediateShader::make_chroma_filter_shader();
	composite_chrominance_filter_shader_program->set_source_texture_unit(filtered_y_texture_unit);
	composite_chrominance_filter_shader_program->set_output_size(IntermediateBufferWidth, IntermediateBufferHeight);
}

void OpenGLOutputBuilder::prepare_rgb_input_shaders()
{
	if(_rgb_shader)
	{
		rgb_input_shader_program = OpenGL::IntermediateShader::make_rgb_source_shader(_rgb_shader);
		rgb_input_shader_program->set_source_texture_unit(source_data_texture_unit);
		rgb_input_shader_program->set_output_size(IntermediateBufferWidth, IntermediateBufferHeight);

		rgb_filter_shader_program = OpenGL::IntermediateShader::make_rgb_filter_shader();
		rgb_filter_shader_program->set_source_texture_unit(composite_texture_unit);
		rgb_filter_shader_program->set_output_size(IntermediateBufferWidth, IntermediateBufferHeight);
	}
}

void OpenGLOutputBuilder::prepare_source_vertex_array()
{
	if(composite_input_shader_program)
	{
		GLint inputPositionAttribute		= composite_input_shader_program->get_attrib_location("inputPosition");
		GLint outputPositionAttribute		= composite_input_shader_program->get_attrib_location("outputPosition");
		GLint phaseAndAmplitudeAttribute	= composite_input_shader_program->get_attrib_location("phaseAndAmplitude");
		GLint phaseTimeAttribute			= composite_input_shader_program->get_attrib_location("phaseTime");

		glBindVertexArray(source_vertex_array);

		glEnableVertexAttribArray((GLuint)inputPositionAttribute);
		glEnableVertexAttribArray((GLuint)outputPositionAttribute);
		glEnableVertexAttribArray((GLuint)phaseAndAmplitudeAttribute);
		glEnableVertexAttribArray((GLuint)phaseTimeAttribute);

		const GLsizei vertexStride = SourceVertexSize;
		glBindBuffer(GL_ARRAY_BUFFER, source_array_buffer);
		glVertexAttribPointer((GLuint)inputPositionAttribute,		2, GL_UNSIGNED_SHORT,	GL_FALSE,	vertexStride, (void *)SourceVertexOffsetOfInputPosition);
		glVertexAttribPointer((GLuint)outputPositionAttribute,		2, GL_UNSIGNED_SHORT,	GL_FALSE,	vertexStride, (void *)SourceVertexOffsetOfOutputPosition);
		glVertexAttribPointer((GLuint)phaseAndAmplitudeAttribute,	2, GL_UNSIGNED_BYTE,	GL_TRUE,	vertexStride, (void *)SourceVertexOffsetOfPhaseAndAmplitude);
		glVertexAttribPointer((GLuint)phaseTimeAttribute,			2, GL_UNSIGNED_SHORT,	GL_FALSE,	vertexStride, (void *)SourceVertexOffsetOfPhaseTime);
	}
}

void OpenGLOutputBuilder::prepare_output_shader()
{
	output_shader_program = OpenGL::OutputShader::make_shader("", "texture(texID, srcCoordinatesVarying).rgb", false);
	output_shader_program->set_source_texture_unit(filtered_texture_unit);
}

void OpenGLOutputBuilder::prepare_output_vertex_array()
{
	if(output_shader_program)
	{
		GLint positionAttribute				= output_shader_program->get_attrib_location("position");
		GLint textureCoordinatesAttribute	= output_shader_program->get_attrib_location("srcCoordinates");

		glBindVertexArray(output_vertex_array);

		glEnableVertexAttribArray((GLuint)positionAttribute);
		glEnableVertexAttribArray((GLuint)textureCoordinatesAttribute);

		const GLsizei vertexStride = OutputVertexSize;
		glBindBuffer(GL_ARRAY_BUFFER, output_array_buffer);
		glVertexAttribPointer((GLuint)positionAttribute,			2, GL_UNSIGNED_SHORT,	GL_FALSE,	vertexStride, (void *)OutputVertexOffsetOfPosition);
		glVertexAttribPointer((GLuint)textureCoordinatesAttribute,	2, GL_UNSIGNED_SHORT,	GL_FALSE,	vertexStride, (void *)OutputVertexOffsetOfTexCoord);
	}
}

#pragma mark - Public Configuration

void OpenGLOutputBuilder::set_output_device(OutputDevice output_device)
{
	if(_output_device != output_device)
	{
		_output_device = output_device;
		_composite_src_output_y = 0;
		_last_output_width = 0;
		_last_output_height = 0;
	}
}

void OpenGLOutputBuilder::set_timing(unsigned int input_frequency, unsigned int cycles_per_line, unsigned int height_of_display, unsigned int horizontal_scan_period, unsigned int vertical_scan_period, unsigned int vertical_period_divider)
{
	_output_mutex->lock();
	_input_frequency = input_frequency;
	_cycles_per_line = cycles_per_line;
	_height_of_display = height_of_display;
	_horizontal_scan_period = horizontal_scan_period;
	_vertical_scan_period = vertical_scan_period;
	_vertical_period_divider = vertical_period_divider;

	set_timing_uniforms();
	_output_mutex->unlock();
}

#pragma mark - Internal Configuration

void OpenGLOutputBuilder::set_colour_space_uniforms()
{
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

	if(composite_input_shader_program)				composite_input_shader_program->set_colour_conversion_matrices(fromRGB, toRGB);
	if(composite_chrominance_filter_shader_program) composite_chrominance_filter_shader_program->set_colour_conversion_matrices(fromRGB, toRGB);
}

void OpenGLOutputBuilder::set_timing_uniforms()
{
	OpenGL::IntermediateShader *intermediate_shaders[] = {
		composite_input_shader_program.get(),
		composite_separation_filter_program.get(),
		composite_y_filter_shader_program.get(),
		composite_chrominance_filter_shader_program.get()
	};
	bool extends = false;
	float phaseCyclesPerTick = (float)_colour_cycle_numerator / (float)(_colour_cycle_denominator * _cycles_per_line);
	for(int c = 0; c < 3; c++)
	{
		if(intermediate_shaders[c]) intermediate_shaders[c]->set_phase_cycles_per_sample(phaseCyclesPerTick, extends);
		extends = true;
	}

	if(output_shader_program) output_shader_program->set_timing(_height_of_display, _cycles_per_line, _horizontal_scan_period, _vertical_scan_period, _vertical_period_divider);

	float colour_subcarrier_frequency = (float)_colour_cycle_numerator / (float)_colour_cycle_denominator;
	if(composite_separation_filter_program)			composite_separation_filter_program->set_separation_frequency(_cycles_per_line, colour_subcarrier_frequency);
	if(composite_y_filter_shader_program)			composite_y_filter_shader_program->set_filter_coefficients(_cycles_per_line, colour_subcarrier_frequency * 0.66f);
	if(composite_chrominance_filter_shader_program)	composite_chrominance_filter_shader_program->set_filter_coefficients(_cycles_per_line, colour_subcarrier_frequency * 0.5f);
	if(rgb_filter_shader_program)					rgb_filter_shader_program->set_filter_coefficients(_cycles_per_line, (float)_input_frequency * 0.5f);
}
