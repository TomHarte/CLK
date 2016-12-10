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
	visible_area_(Rect(0, 0, 1, 1)),
	composite_src_output_y_(0),
	composite_shader_(nullptr),
	rgb_shader_(nullptr),
	last_output_width_(0),
	last_output_height_(0),
	fence_(nullptr),
	texture_builder(bytes_per_pixel, source_data_texture_unit),
	array_builder(SourceVertexBufferDataSize, OutputVertexBufferDataSize),
	composite_texture_(IntermediateBufferWidth, IntermediateBufferHeight, composite_texture_unit),
	separated_texture_(IntermediateBufferWidth, IntermediateBufferHeight, separated_texture_unit),
	filtered_y_texture_(IntermediateBufferWidth, IntermediateBufferHeight, filtered_y_texture_unit),
	filtered_texture_(IntermediateBufferWidth, IntermediateBufferHeight, filtered_texture_unit)
{
	glBlendFunc(GL_SRC_ALPHA, GL_CONSTANT_COLOR);
	glBlendColor(0.6f, 0.6f, 0.6f, 1.0f);

	// create the output vertex array
	glGenVertexArrays(1, &output_vertex_array_);

	// create the source vertex array
	glGenVertexArrays(1, &source_vertex_array_);
}

OpenGLOutputBuilder::~OpenGLOutputBuilder()
{
	glDeleteVertexArrays(1, &output_vertex_array_);

	free(composite_shader_);
	free(rgb_shader_);
}

void OpenGLOutputBuilder::draw_frame(unsigned int output_width, unsigned int output_height, bool only_if_dirty)
{
	// lock down any other draw_frames
	draw_mutex_.lock();

	// establish essentials
	if(!output_shader_program_)
	{
		prepare_composite_input_shaders();
		prepare_rgb_input_shaders();
		prepare_source_vertex_array();

		prepare_output_shader();
		prepare_output_vertex_array();

		set_timing_uniforms();
		set_colour_space_uniforms();
	}

	if(fence_ != nullptr)
	{
		// if the GPU is still busy, don't wait; we'll catch it next time
		if(glClientWaitSync(fence_, GL_SYNC_FLUSH_COMMANDS_BIT, only_if_dirty ? 0 : GL_TIMEOUT_IGNORED) == GL_TIMEOUT_EXPIRED)
		{
			draw_mutex_.unlock();
			return;
		}

		glDeleteSync(fence_);
	}

	// make sure there's a target to draw to
	if(!framebuffer_ || framebuffer_->get_height() != output_height || framebuffer_->get_width() != output_width)
	{
		std::unique_ptr<OpenGL::TextureTarget> new_framebuffer(new OpenGL::TextureTarget((GLsizei)output_width, (GLsizei)output_height, pixel_accumulation_texture_unit));
		if(framebuffer_)
		{
			new_framebuffer->bind_framebuffer();
			glClear(GL_COLOR_BUFFER_BIT);

			glActiveTexture(pixel_accumulation_texture_unit);
			framebuffer_->bind_texture();
			framebuffer_->draw((float)output_width / (float)output_height);

			new_framebuffer->bind_texture();
		}
		framebuffer_ = std::move(new_framebuffer);
	}

	// lock out the machine emulation until data is copied
	output_mutex_.lock();

	// release the mapping, giving up on trying to draw if data has been lost
	ArrayBuilder::Submission array_submission = array_builder.submit();

	// upload new source pixels, if any
	glActiveTexture(source_data_texture_unit);
	texture_builder.submit();

	// buffer usage restart from 0 for the next time around
	composite_src_output_y_ = 0;

	// data having been grabbed, allow the machine to continue
	output_mutex_.unlock();

	struct RenderStage {
		OpenGL::TextureTarget *const target;
		OpenGL::Shader *const shader;
		float clear_colour[3];
	};

	// for composite video, go through four steps to get to something that can be painted to the output
	RenderStage composite_render_stages[] =
	{
		{&composite_texture_,	composite_input_shader_program_.get(),				{0.0, 0.0, 0.0}},
		{&separated_texture_,	composite_separation_filter_program_.get(),			{0.0, 0.5, 0.5}},
		{&filtered_y_texture_,	composite_y_filter_shader_program_.get(),			{0.0, 0.5, 0.5}},
		{&filtered_texture_,	composite_chrominance_filter_shader_program_.get(),	{0.0, 0.0, 0.0}},
		{nullptr}
	};

	// for RGB video, there's only two steps
	RenderStage rgb_render_stages[] =
	{
		{&composite_texture_,	rgb_input_shader_program_.get(),	{0.0, 0.0, 0.0}},
		{&filtered_texture_,	rgb_filter_shader_program_.get(),	{0.0, 0.0, 0.0}},
		{nullptr}
	};

	RenderStage *active_pipeline = (output_device_ == Television || !rgb_input_shader_program_) ? composite_render_stages : rgb_render_stages;

	if(array_submission.input_size || array_submission.output_size)
	{
		// all drawing will be from the source vertex array and without blending
		glBindVertexArray(source_vertex_array_);
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
		framebuffer_->bind_framebuffer();

		// draw from the output array buffer, with blending
		glBindVertexArray(output_vertex_array_);
		glEnable(GL_BLEND);

		// update uniforms, then bind the target
		if(last_output_width_ != output_width || last_output_height_ != output_height)
		{
			output_shader_program_->set_output_size(output_width, output_height, visible_area_);
			last_output_width_ = output_width;
			last_output_height_ = output_height;
		}
		output_shader_program_->bind();

		// draw
		glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)array_submission.output_size / OutputVertexSize);
	}

	// copy framebuffer to the intended place
	glDisable(GL_BLEND);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, (GLsizei)output_width, (GLsizei)output_height);

	glActiveTexture(pixel_accumulation_texture_unit);
	framebuffer_->bind_texture();
	framebuffer_->draw((float)output_width / (float)output_height);

	fence_ = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
	draw_mutex_.unlock();
}

void OpenGLOutputBuilder::reset_all_OpenGL_state()
{
	composite_input_shader_program_ = nullptr;
	composite_separation_filter_program_ = nullptr;
	composite_y_filter_shader_program_ = nullptr;
	composite_chrominance_filter_shader_program_ = nullptr;
	rgb_input_shader_program_ = nullptr;
	rgb_filter_shader_program_ = nullptr;
	output_shader_program_ = nullptr;
	framebuffer_ = nullptr;
	last_output_width_ = last_output_height_ = 0;
}

void OpenGLOutputBuilder::set_openGL_context_will_change(bool should_delete_resources)
{
	output_mutex_.lock();
	reset_all_OpenGL_state();
	output_mutex_.unlock();
}

void OpenGLOutputBuilder::set_composite_sampling_function(const char *shader)
{
	output_mutex_.lock();
	composite_shader_ = strdup(shader);
	reset_all_OpenGL_state();
	output_mutex_.unlock();
}

void OpenGLOutputBuilder::set_rgb_sampling_function(const char *shader)
{
	output_mutex_.lock();
	rgb_shader_ = strdup(shader);
	reset_all_OpenGL_state();
	output_mutex_.unlock();
}

#pragma mark - Program compilation

void OpenGLOutputBuilder::prepare_composite_input_shaders()
{
	composite_input_shader_program_ = OpenGL::IntermediateShader::make_source_conversion_shader(composite_shader_, rgb_shader_);
	composite_input_shader_program_->set_source_texture_unit(source_data_texture_unit);
	composite_input_shader_program_->set_output_size(IntermediateBufferWidth, IntermediateBufferHeight);

	composite_separation_filter_program_ = OpenGL::IntermediateShader::make_chroma_luma_separation_shader();
	composite_separation_filter_program_->set_source_texture_unit(composite_texture_unit);
	composite_separation_filter_program_->set_output_size(IntermediateBufferWidth, IntermediateBufferHeight);

	composite_y_filter_shader_program_ = OpenGL::IntermediateShader::make_luma_filter_shader();
	composite_y_filter_shader_program_->set_source_texture_unit(separated_texture_unit);
	composite_y_filter_shader_program_->set_output_size(IntermediateBufferWidth, IntermediateBufferHeight);

	composite_chrominance_filter_shader_program_ = OpenGL::IntermediateShader::make_chroma_filter_shader();
	composite_chrominance_filter_shader_program_->set_source_texture_unit(filtered_y_texture_unit);
	composite_chrominance_filter_shader_program_->set_output_size(IntermediateBufferWidth, IntermediateBufferHeight);
}

void OpenGLOutputBuilder::prepare_rgb_input_shaders()
{
	if(rgb_shader_)
	{
		rgb_input_shader_program_ = OpenGL::IntermediateShader::make_rgb_source_shader(rgb_shader_);
		rgb_input_shader_program_->set_source_texture_unit(source_data_texture_unit);
		rgb_input_shader_program_->set_output_size(IntermediateBufferWidth, IntermediateBufferHeight);

		rgb_filter_shader_program_ = OpenGL::IntermediateShader::make_rgb_filter_shader();
		rgb_filter_shader_program_->set_source_texture_unit(composite_texture_unit);
		rgb_filter_shader_program_->set_output_size(IntermediateBufferWidth, IntermediateBufferHeight);
	}
}

void OpenGLOutputBuilder::prepare_source_vertex_array()
{
	if(composite_input_shader_program_)
	{
		glBindVertexArray(source_vertex_array_);
		array_builder.bind_input();

		composite_input_shader_program_->enable_vertex_attribute_with_pointer("inputStart", 2, GL_UNSIGNED_SHORT, GL_FALSE, SourceVertexSize, (void *)SourceVertexOffsetOfInputStart, 1);
		composite_input_shader_program_->enable_vertex_attribute_with_pointer("outputStart", 2, GL_UNSIGNED_SHORT, GL_FALSE, SourceVertexSize, (void *)SourceVertexOffsetOfOutputStart, 1);
		composite_input_shader_program_->enable_vertex_attribute_with_pointer("ends", 2, GL_UNSIGNED_SHORT, GL_FALSE, SourceVertexSize, (void *)SourceVertexOffsetOfEnds, 1);
		composite_input_shader_program_->enable_vertex_attribute_with_pointer("phaseTimeAndAmplitude", 3, GL_UNSIGNED_BYTE, GL_FALSE, SourceVertexSize, (void *)SourceVertexOffsetOfPhaseTimeAndAmplitude, 1);
	}
}

void OpenGLOutputBuilder::prepare_output_shader()
{
	output_shader_program_ = OpenGL::OutputShader::make_shader("", "texture(texID, srcCoordinatesVarying).rgb", false);
	output_shader_program_->set_source_texture_unit(filtered_texture_unit);
}

void OpenGLOutputBuilder::prepare_output_vertex_array()
{
	if(output_shader_program_)
	{
		glBindVertexArray(output_vertex_array_);
		array_builder.bind_output();
		output_shader_program_->enable_vertex_attribute_with_pointer("horizontal", 2, GL_UNSIGNED_SHORT, GL_FALSE, OutputVertexSize, (void *)OutputVertexOffsetOfHorizontal, 1);
		output_shader_program_->enable_vertex_attribute_with_pointer("vertical", 2, GL_UNSIGNED_SHORT, GL_FALSE, OutputVertexSize, (void *)OutputVertexOffsetOfVertical, 1);
	}
}

#pragma mark - Public Configuration

void OpenGLOutputBuilder::set_output_device(OutputDevice output_device)
{
	if(output_device_ != output_device)
	{
		output_device_ = output_device;
		composite_src_output_y_ = 0;
		last_output_width_ = 0;
		last_output_height_ = 0;
	}
}

void OpenGLOutputBuilder::set_timing(unsigned int input_frequency, unsigned int cycles_per_line, unsigned int height_of_display, unsigned int horizontal_scan_period, unsigned int vertical_scan_period, unsigned int vertical_period_divider)
{
	output_mutex_.lock();
	input_frequency_ = input_frequency;
	cycles_per_line_ = cycles_per_line;
	height_of_display_ = height_of_display;
	horizontal_scan_period_ = horizontal_scan_period;
	vertical_scan_period_ = vertical_scan_period;
	vertical_period_divider_ = vertical_period_divider;

	set_timing_uniforms();
	output_mutex_.unlock();
}

#pragma mark - Internal Configuration

void OpenGLOutputBuilder::set_colour_space_uniforms()
{
	GLfloat rgbToYUV[] = {0.299f, -0.14713f, 0.615f, 0.587f, -0.28886f, -0.51499f, 0.114f, 0.436f, -0.10001f};
	GLfloat yuvToRGB[] = {1.0f, 1.0f, 1.0f, 0.0f, -0.39465f, 2.03211f, 1.13983f, -0.58060f, 0.0f};

	GLfloat rgbToYIQ[] = {0.299f, 0.596f, 0.211f, 0.587f, -0.274f, -0.523f, 0.114f, -0.322f, 0.312f};
	GLfloat yiqToRGB[] = {1.0f, 1.0f, 1.0f, 0.956f, -0.272f, -1.106f, 0.621f, -0.647f, 1.703f};

	GLfloat *fromRGB, *toRGB;

	switch(colour_space_)
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

	if(composite_input_shader_program_)					composite_input_shader_program_->set_colour_conversion_matrices(fromRGB, toRGB);
	if(composite_chrominance_filter_shader_program_)	composite_chrominance_filter_shader_program_->set_colour_conversion_matrices(fromRGB, toRGB);
}

void OpenGLOutputBuilder::set_timing_uniforms()
{
	OpenGL::IntermediateShader *intermediate_shaders[] = {
		composite_input_shader_program_.get(),
		composite_separation_filter_program_.get(),
		composite_y_filter_shader_program_.get(),
		composite_chrominance_filter_shader_program_.get()
	};
	bool extends = false;
	float phaseCyclesPerTick = (float)colour_cycle_numerator_ / (float)(colour_cycle_denominator_ * cycles_per_line_);
	for(int c = 0; c < 3; c++)
	{
		if(intermediate_shaders[c]) intermediate_shaders[c]->set_phase_cycles_per_sample(phaseCyclesPerTick, extends);
		extends = true;
	}

	if(output_shader_program_) output_shader_program_->set_timing(height_of_display_, cycles_per_line_, horizontal_scan_period_, vertical_scan_period_, vertical_period_divider_);

	float colour_subcarrier_frequency = (float)colour_cycle_numerator_ / (float)colour_cycle_denominator_;
	if(composite_separation_filter_program_)			composite_separation_filter_program_->set_separation_frequency(cycles_per_line_, colour_subcarrier_frequency);
	if(composite_y_filter_shader_program_)				composite_y_filter_shader_program_->set_filter_coefficients(cycles_per_line_, colour_subcarrier_frequency * 0.25f);
	if(composite_chrominance_filter_shader_program_)	composite_chrominance_filter_shader_program_->set_filter_coefficients(cycles_per_line_, colour_subcarrier_frequency * 0.5f);
	if(rgb_filter_shader_program_)						rgb_filter_shader_program_->set_filter_coefficients(cycles_per_line_, (float)input_frequency_ * 0.5f);
}
