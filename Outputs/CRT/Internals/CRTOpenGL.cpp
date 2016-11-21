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

using namespace Outputs::CRT;

namespace {
	static const GLenum composite_texture_unit			= GL_TEXTURE0;
	static const GLenum separated_texture_unit			= GL_TEXTURE1;
	static const GLenum filtered_y_texture_unit			= GL_TEXTURE2;
	static const GLenum filtered_texture_unit			= GL_TEXTURE3;
	static const GLenum source_data_texture_unit		= GL_TEXTURE4;
	static const GLenum pixel_accumulation_texture_unit	= GL_TEXTURE5;
}

OpenGLOutputBuilder::OpenGLOutputBuilder(size_t bytes_per_pixel) :
	_output_mutex(new std::mutex),
	_draw_mutex(new std::mutex),
	_visible_area(Rect(0, 0, 1, 1)),
	_composite_src_output_y(0),
	_composite_shader(nullptr),
	_rgb_shader(nullptr),
	_last_output_width(0),
	_last_output_height(0),
	_fence(nullptr),
	texture_builder(bytes_per_pixel, source_data_texture_unit),
	array_builder(SourceVertexBufferDataSize, OutputVertexBufferDataSize)
{
	glBlendFunc(GL_SRC_ALPHA, GL_CONSTANT_COLOR);
	glBlendColor(0.6f, 0.6f, 0.6f, 1.0f);

	// Create intermediate textures and bind to slots 0, 1 and 2
	compositeTexture.reset(new OpenGL::TextureTarget(IntermediateBufferWidth, IntermediateBufferHeight, composite_texture_unit));
	separatedTexture.reset(new OpenGL::TextureTarget(IntermediateBufferWidth, IntermediateBufferHeight, separated_texture_unit));
	filteredYTexture.reset(new OpenGL::TextureTarget(IntermediateBufferWidth, IntermediateBufferHeight, filtered_y_texture_unit));
	filteredTexture.reset(new OpenGL::TextureTarget(IntermediateBufferWidth, IntermediateBufferHeight, filtered_texture_unit));

	// create the output vertex array
	glGenVertexArrays(1, &output_vertex_array);

	// create the source vertex array
	glGenVertexArrays(1, &source_vertex_array);
}

OpenGLOutputBuilder::~OpenGLOutputBuilder()
{
	glDeleteVertexArrays(1, &output_vertex_array);

	free(_composite_shader);
	free(_rgb_shader);
}

void OpenGLOutputBuilder::draw_frame(unsigned int output_width, unsigned int output_height, bool only_if_dirty)
{
	// lock down any other draw_frames
	_draw_mutex->lock();

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
	}

	if(_fence != nullptr)
	{
		// if the GPU is still busy, don't wait; we'll catch it next time
		if(glClientWaitSync(_fence, GL_SYNC_FLUSH_COMMANDS_BIT, only_if_dirty ? 0 : GL_TIMEOUT_IGNORED) == GL_TIMEOUT_EXPIRED)
		{
			_draw_mutex->unlock();
			return;
		}

		glDeleteSync(_fence);
	}

	// make sure there's a target to draw to
	if(!framebuffer || framebuffer->get_height() != output_height || framebuffer->get_width() != output_width)
	{
		std::unique_ptr<OpenGL::TextureTarget> new_framebuffer(new OpenGL::TextureTarget((GLsizei)output_width, (GLsizei)output_height, pixel_accumulation_texture_unit));
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

	// lock out the machine emulation until data is copied
	_output_mutex->lock();

	// release the mapping, giving up on trying to draw if data has been lost
	ArrayBuilder::Submission array_submission = array_builder.submit();

	// upload new source pixels, if any
	glActiveTexture(source_data_texture_unit);
	texture_builder.submit();

	// buffer usage restart from 0 for the next time around
	_composite_src_output_y = 0;

	// data having been grabbed, allow the machine to continue
	_output_mutex->unlock();

	struct RenderStage {
		OpenGL::TextureTarget *const target;
		OpenGL::Shader *const shader;
		float clear_colour[3];
	};

	// for composite video, go through four steps to get to something that can be painted to the output
	RenderStage composite_render_stages[] =
	{
		{compositeTexture.get(),	composite_input_shader_program.get(),				{0.0, 0.0, 0.0}},
		{separatedTexture.get(),	composite_separation_filter_program.get(),			{0.0, 0.5, 0.5}},
		{filteredYTexture.get(),	composite_y_filter_shader_program.get(),			{0.0, 0.5, 0.5}},
		{filteredTexture.get(),		composite_chrominance_filter_shader_program.get(),	{0.0, 0.0, 0.0}},
		{nullptr}
	};

	// for RGB video, there's only two steps
	RenderStage rgb_render_stages[] =
	{
		{compositeTexture.get(),	rgb_input_shader_program.get(),		{0.0, 0.0, 0.0}},
		{filteredTexture.get(),		rgb_filter_shader_program.get(),	{0.0, 0.0, 0.0}},
		{nullptr}
	};

	RenderStage *active_pipeline = (_output_device == Television || !rgb_input_shader_program) ? composite_render_stages : rgb_render_stages;

	if(array_submission.input_size || array_submission.output_size)
	{
		// all drawing will be from the source vertex array and without blending
		glBindVertexArray(source_vertex_array);
		glDisable(GL_BLEND);

		while(active_pipeline->target)
		{
			// switch to the framebuffer and shader associated with this stage
			active_pipeline->shader->bind();
			active_pipeline->target->bind_framebuffer();

			// if this is the final stage before painting to the CRT, clear the framebuffer before drawing in order to blank out
			// those portions for which no input was provided
			if(!active_pipeline[1].target)
			{
				glClearColor(active_pipeline->clear_colour[0], active_pipeline->clear_colour[1], active_pipeline->clear_colour[2], 1.0f);
				glClear(GL_COLOR_BUFFER_BIT);
			}

			// draw
			glDrawArraysInstanced(GL_LINES, 0, 2, (GLsizei)array_submission.input_size / SourceVertexSize);

			active_pipeline++;
		}

		// prepare to transfer to framebuffer
		framebuffer->bind_framebuffer();

		// draw from the output array buffer, with blending
		glBindVertexArray(output_vertex_array);
		glEnable(GL_BLEND);

		// update uniforms, then bind the target
		if(_last_output_width != output_width || _last_output_height != output_height)
		{
			output_shader_program->set_output_size(output_width, output_height, _visible_area);
			_last_output_width = output_width;
			_last_output_height = output_height;
		}
		output_shader_program->bind();

		// draw
		glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)array_submission.output_size / OutputVertexSize);
	}

	// copy framebuffer to the intended place
	glDisable(GL_BLEND);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, (GLsizei)output_width, (GLsizei)output_height);

	glActiveTexture(pixel_accumulation_texture_unit);
	framebuffer->bind_texture();
	framebuffer->draw((float)output_width / (float)output_height);

	_fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
	_draw_mutex->unlock();
}

void OpenGLOutputBuilder::reset_all_OpenGL_state()
{
	composite_input_shader_program = nullptr;
	composite_separation_filter_program = nullptr;
	composite_y_filter_shader_program = nullptr;
	composite_chrominance_filter_shader_program = nullptr;
	rgb_input_shader_program = nullptr;
	rgb_filter_shader_program = nullptr;
	output_shader_program = nullptr;
	framebuffer = nullptr;
	_last_output_width = _last_output_height = 0;
}

void OpenGLOutputBuilder::set_openGL_context_will_change(bool should_delete_resources)
{
	_output_mutex->lock();
	reset_all_OpenGL_state();
	_output_mutex->unlock();
}

void OpenGLOutputBuilder::set_composite_sampling_function(const char *shader)
{
	_output_mutex->lock();
	_composite_shader = strdup(shader);
	reset_all_OpenGL_state();
	_output_mutex->unlock();
}

void OpenGLOutputBuilder::set_rgb_sampling_function(const char *shader)
{
	_output_mutex->lock();
	_rgb_shader = strdup(shader);
	reset_all_OpenGL_state();
	_output_mutex->unlock();
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
		glBindVertexArray(source_vertex_array);
		array_builder.bind_input();

		composite_input_shader_program->enable_vertex_attribute_with_pointer("inputStart", 2, GL_UNSIGNED_SHORT, GL_FALSE, SourceVertexSize, (void *)SourceVertexOffsetOfInputStart, 1);
		composite_input_shader_program->enable_vertex_attribute_with_pointer("outputStart", 2, GL_UNSIGNED_SHORT, GL_FALSE, SourceVertexSize, (void *)SourceVertexOffsetOfOutputStart, 1);
		composite_input_shader_program->enable_vertex_attribute_with_pointer("ends", 2, GL_UNSIGNED_SHORT, GL_FALSE, SourceVertexSize, (void *)SourceVertexOffsetOfEnds, 1);
		composite_input_shader_program->enable_vertex_attribute_with_pointer("phaseTimeAndAmplitude", 3, GL_UNSIGNED_BYTE, GL_FALSE, SourceVertexSize, (void *)SourceVertexOffsetOfPhaseTimeAndAmplitude, 1);
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
		glBindVertexArray(output_vertex_array);
		array_builder.bind_output();
		output_shader_program->enable_vertex_attribute_with_pointer("horizontal", 2, GL_UNSIGNED_SHORT, GL_FALSE, OutputVertexSize, (void *)OutputVertexOffsetOfHorizontal, 1);
		output_shader_program->enable_vertex_attribute_with_pointer("vertical", 2, GL_UNSIGNED_SHORT, GL_FALSE, OutputVertexSize, (void *)OutputVertexOffsetOfVertical, 1);
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
