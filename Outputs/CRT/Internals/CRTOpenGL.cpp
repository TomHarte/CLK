//  CRTOpenGL.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/02/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "../CRT.hpp"

#include <cassert>
#include <cmath>
#include <cstdlib>

#include "CRTOpenGL.hpp"
#include "../../../SignalProcessing/FIRFilter.hpp"
#include "Shaders/OutputShader.hpp"

using namespace Outputs::CRT;

namespace {
	static const GLenum source_data_texture_unit		= GL_TEXTURE0;
	static const GLenum pixel_accumulation_texture_unit	= GL_TEXTURE1;

	static const GLenum composite_texture_unit			= GL_TEXTURE2;
	static const GLenum separated_texture_unit			= GL_TEXTURE3;
	static const GLenum filtered_texture_unit			= GL_TEXTURE4;

	static const GLenum work_texture_unit				= GL_TEXTURE2;
}

OpenGLOutputBuilder::OpenGLOutputBuilder(std::size_t bytes_per_pixel) :
		visible_area_(Rect(0, 0, 1, 1)),
		composite_src_output_y_(0),
		last_output_width_(0),
		last_output_height_(0),
		fence_(nullptr),
		texture_builder(bytes_per_pixel, source_data_texture_unit),
		array_builder(SourceVertexBufferDataSize, OutputVertexBufferDataSize) {
	glBlendFunc(GL_SRC_ALPHA, GL_CONSTANT_COLOR);
	glBlendColor(0.6f, 0.6f, 0.6f, 1.0f);

	// create the output vertex array
	glGenVertexArrays(1, &output_vertex_array_);

	// create the source vertex array
	glGenVertexArrays(1, &source_vertex_array_);

//	bool supports_texture_barrier = false;
#ifdef GL_NV_texture_barrier
//	GLint number_of_extensions;
//	glGetIntegerv(GL_NUM_EXTENSIONS, &number_of_extensions);
//
//	for(GLuint c = 0; c < (GLuint)number_of_extensions; c++) {
//		const char *extension_name = (const char *)glGetStringi(GL_EXTENSIONS, c);
//		if(!std::strcmp(extension_name, "GL_NV_texture_barrier")) {
//			supports_texture_barrier = true;
//		}
//	}
#endif

//	if(supports_texture_barrier) {
//		work_texture_.reset(new OpenGL::TextureTarget(IntermediateBufferWidth, IntermediateBufferHeight*2, work_texture_unit));
//	} else {
		composite_texture_.reset(new OpenGL::TextureTarget(IntermediateBufferWidth, IntermediateBufferHeight, composite_texture_unit, GL_NEAREST));
		separated_texture_.reset(new OpenGL::TextureTarget(IntermediateBufferWidth, IntermediateBufferHeight, separated_texture_unit, GL_NEAREST));
		filtered_texture_.reset(new OpenGL::TextureTarget(IntermediateBufferWidth, IntermediateBufferHeight, filtered_texture_unit, GL_LINEAR));
//	}
}

OpenGLOutputBuilder::~OpenGLOutputBuilder() {
	glDeleteVertexArrays(1, &output_vertex_array_);
}

void OpenGLOutputBuilder::set_target_framebuffer(GLint target_framebuffer) {
	target_framebuffer_ = target_framebuffer;
}

void OpenGLOutputBuilder::draw_frame(unsigned int output_width, unsigned int output_height, bool only_if_dirty) {
	// lock down any other draw_frames
	draw_mutex_.lock();

	// establish essentials
	if(!output_shader_program_) {
		prepare_composite_input_shaders();
		prepare_svideo_input_shaders();
		prepare_rgb_input_shaders();
		prepare_source_vertex_array();

		prepare_output_shader();
		prepare_output_vertex_array();

		set_timing_uniforms();
		set_colour_space_uniforms();
		set_gamma();
	}

	if(fence_ != nullptr) {
		// if the GPU is still busy, don't wait; we'll catch it next time
		if(glClientWaitSync(fence_, GL_SYNC_FLUSH_COMMANDS_BIT, only_if_dirty ? 0 : GL_TIMEOUT_IGNORED) == GL_TIMEOUT_EXPIRED) {
			draw_mutex_.unlock();
			return;
		}

		glDeleteSync(fence_);
	}

	// make sure everything is bound
	composite_texture_->bind_texture();
	separated_texture_->bind_texture();
	filtered_texture_->bind_texture();
	if(work_texture_) work_texture_->bind_texture();

	// make sure there's a target to draw to
	if(!framebuffer_ || static_cast<unsigned int>(framebuffer_->get_height()) != output_height || static_cast<unsigned int>(framebuffer_->get_width()) != output_width) {
		std::unique_ptr<OpenGL::TextureTarget> new_framebuffer(new OpenGL::TextureTarget((GLsizei)output_width, (GLsizei)output_height, pixel_accumulation_texture_unit, GL_LINEAR));
		if(framebuffer_) {
			new_framebuffer->bind_framebuffer();
			glClear(GL_COLOR_BUFFER_BIT);

			glActiveTexture(pixel_accumulation_texture_unit);
			framebuffer_->bind_texture();
			framebuffer_->draw(static_cast<float>(output_width) / static_cast<float>(output_height));

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
	texture_builder.bind();
	texture_builder.submit();

	// buffer usage restart from 0 for the next time around
	composite_src_output_y_ = 0;

	// data having been grabbed, allow the machine to continue
	output_mutex_.unlock();

	struct RenderStage {
		OpenGL::Shader *const shader;
		OpenGL::TextureTarget *const target;
		float clear_colour[3];
	};

	// for composite video, go through four steps to get to something that can be painted to the output
	const RenderStage composite_render_stages[] = {
		{composite_input_shader_program_.get(),					composite_texture_.get(),		{0.0, 0.0, 0.0}},
		{composite_separation_filter_program_.get(),			separated_texture_.get(),		{0.0, 0.5, 0.5}},
		{composite_chrominance_filter_shader_program_.get(),	filtered_texture_.get(),		{0.0, 0.0, 0.0}},
		{nullptr, nullptr}
	};

	// for s-video, there are two steps — it's like composite but skips separation
	const RenderStage svideo_render_stages[] = {
		{svideo_input_shader_program_.get(),					separated_texture_.get(),		{0.0, 0.5, 0.5}},
		{composite_chrominance_filter_shader_program_.get(),	filtered_texture_.get(),		{0.0, 0.0, 0.0}},
		{nullptr, nullptr}
	};

	// for RGB video, there's also only two steps; a lowpass filter is still applied per physical reality
	const RenderStage rgb_render_stages[] = {
		{rgb_input_shader_program_.get(),	composite_texture_.get(),	{0.0, 0.0, 0.0}},
		{rgb_filter_shader_program_.get(),	filtered_texture_.get(),	{0.0, 0.0, 0.0}},
		{nullptr, nullptr}
	};

	const RenderStage *active_pipeline;
	switch(video_signal_) {
		default:
		case VideoSignal::Composite:	active_pipeline = composite_render_stages;	break;
		case VideoSignal::SVideo:		active_pipeline = svideo_render_stages;		break;
		case VideoSignal::RGB:			active_pipeline = rgb_render_stages;		break;
	}

	if(array_submission.input_size || array_submission.output_size) {
		// all drawing will be from the source vertex array and without blending
		glBindVertexArray(source_vertex_array_);
		glDisable(GL_BLEND);

#ifdef GL_NV_texture_barrier
//		if(work_texture_) {
//			work_texture_->bind_framebuffer();
//			glClear(GL_COLOR_BUFFER_BIT);
//		}
#endif

		while(active_pipeline->shader) {
			// switch to the framebuffer and shader associated with this stage
			active_pipeline->shader->bind();

			if(!work_texture_) {
				active_pipeline->target->bind_framebuffer();

				// if this is the final stage before painting to the CRT, clear the framebuffer before drawing in order to blank out
				// those portions for which no input was provided
//				if(!active_pipeline[1].shader) {
					glClearColor(active_pipeline->clear_colour[0], active_pipeline->clear_colour[1], active_pipeline->clear_colour[2], 1.0f);
					glClear(GL_COLOR_BUFFER_BIT);
//				}
			}

			// draw
			glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)array_submission.input_size / SourceVertexSize);

			active_pipeline++;
#ifdef GL_NV_texture_barrier
//			glTextureBarrierNV();
#endif
		}

		// prepare to transfer to framebuffer
		framebuffer_->bind_framebuffer();

		// draw from the output array buffer, with blending
		glBindVertexArray(output_vertex_array_);
		glEnable(GL_BLEND);

		// update uniforms, then bind the target
		if(last_output_width_ != output_width || last_output_height_ != output_height) {
			output_shader_program_->set_output_size(output_width, output_height, visible_area_);
			last_output_width_ = output_width;
			last_output_height_ = output_height;
		}
		output_shader_program_->bind();

		// draw
		glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)array_submission.output_size / OutputVertexSize);
	}

#ifdef GL_NV_texture_barrier
//	glTextureBarrierNV();
#endif

	// copy framebuffer to the intended place
	glDisable(GL_BLEND);
	glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(target_framebuffer_));
	glViewport(0, 0, (GLsizei)output_width, (GLsizei)output_height);

	glActiveTexture(pixel_accumulation_texture_unit);
	framebuffer_->bind_texture();
	framebuffer_->draw(static_cast<float>(output_width) / static_cast<float>(output_height));

	fence_ = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
	draw_mutex_.unlock();
}

void OpenGLOutputBuilder::reset_all_OpenGL_state() {
	composite_input_shader_program_ = nullptr;
	composite_separation_filter_program_ = nullptr;
	composite_chrominance_filter_shader_program_ = nullptr;
	svideo_input_shader_program_ = nullptr;
	rgb_input_shader_program_ = nullptr;
	rgb_filter_shader_program_ = nullptr;
	output_shader_program_ = nullptr;
	framebuffer_ = nullptr;
	last_output_width_ = last_output_height_ = 0;
}

void OpenGLOutputBuilder::set_openGL_context_will_change(bool should_delete_resources) {
	output_mutex_.lock();
	reset_all_OpenGL_state();
	output_mutex_.unlock();
}

void OpenGLOutputBuilder::set_composite_sampling_function(const std::string &shader) {
	std::lock_guard<std::mutex> lock_guard(output_mutex_);
	composite_shader_ = shader;
	reset_all_OpenGL_state();
}

void OpenGLOutputBuilder::set_svideo_sampling_function(const std::string &shader) {
	std::lock_guard<std::mutex> lock_guard(output_mutex_);
	svideo_shader_ = shader;
	reset_all_OpenGL_state();
}

void OpenGLOutputBuilder::set_rgb_sampling_function(const std::string &shader) {
	std::lock_guard<std::mutex> lock_guard(output_mutex_);
	rgb_shader_ = shader;
	reset_all_OpenGL_state();
}

// MARK: - Program compilation

void OpenGLOutputBuilder::prepare_composite_input_shaders() {
	composite_input_shader_program_ = OpenGL::IntermediateShader::make_composite_source_shader(composite_shader_, svideo_shader_, rgb_shader_);
	composite_input_shader_program_->set_source_texture_unit(source_data_texture_unit);
	composite_input_shader_program_->set_output_size(IntermediateBufferWidth, IntermediateBufferHeight);

	composite_separation_filter_program_ = OpenGL::IntermediateShader::make_chroma_luma_separation_shader();
	composite_separation_filter_program_->set_source_texture_unit(work_texture_ ? work_texture_unit : composite_texture_unit);
	composite_separation_filter_program_->set_output_size(IntermediateBufferWidth, IntermediateBufferHeight);

	composite_chrominance_filter_shader_program_ = OpenGL::IntermediateShader::make_chroma_filter_shader();
	composite_chrominance_filter_shader_program_->set_source_texture_unit(work_texture_ ? work_texture_unit : separated_texture_unit);
	composite_chrominance_filter_shader_program_->set_output_size(IntermediateBufferWidth, IntermediateBufferHeight);

	// TODO: the below is related to texture fencing, which is not yet implemented correctly, so not yet enabled.
	if(work_texture_) {
		composite_input_shader_program_->set_is_double_height(true, 0.0f, 0.0f);
		composite_separation_filter_program_->set_is_double_height(true, 0.0f, 0.5f);
		composite_chrominance_filter_shader_program_->set_is_double_height(true, 0.5f, 0.0f);
	} else {
		composite_input_shader_program_->set_is_double_height(false);
		composite_separation_filter_program_->set_is_double_height(false);
		composite_chrominance_filter_shader_program_->set_is_double_height(false);
	}
}

void OpenGLOutputBuilder::prepare_svideo_input_shaders() {
	if(!svideo_shader_.empty() || !rgb_shader_.empty()) {
		svideo_input_shader_program_ = OpenGL::IntermediateShader::make_svideo_source_shader(svideo_shader_, rgb_shader_);
		svideo_input_shader_program_->set_source_texture_unit(source_data_texture_unit);
		svideo_input_shader_program_->set_output_size(IntermediateBufferWidth, IntermediateBufferHeight);

		// TODO: the below is related to texture fencing, which is not yet implemented correctly, so not yet enabled.
		if(work_texture_) {
			svideo_input_shader_program_->set_is_double_height(true, 0.0f, 0.0f);
		} else {
			svideo_input_shader_program_->set_is_double_height(false);
		}
	}
}

void OpenGLOutputBuilder::prepare_rgb_input_shaders() {
	if(!rgb_shader_.empty()) {
		rgb_input_shader_program_ = OpenGL::IntermediateShader::make_rgb_source_shader(rgb_shader_);
		rgb_input_shader_program_->set_source_texture_unit(source_data_texture_unit);
		rgb_input_shader_program_->set_output_size(IntermediateBufferWidth, IntermediateBufferHeight);

		rgb_filter_shader_program_ = OpenGL::IntermediateShader::make_rgb_filter_shader();
		rgb_filter_shader_program_->set_source_texture_unit(composite_texture_unit);
		rgb_filter_shader_program_->set_output_size(IntermediateBufferWidth, IntermediateBufferHeight);
	}
}

void OpenGLOutputBuilder::prepare_source_vertex_array() {
	if(composite_input_shader_program_ || svideo_input_shader_program_) {
		glBindVertexArray(source_vertex_array_);
		array_builder.bind_input();
	}

	using Shader = OpenGL::IntermediateShader;
	OpenGL::IntermediateShader *const shaders[] = {
		composite_input_shader_program_.get(),
		svideo_input_shader_program_.get()
	};
	for(int c = 0; c < 2; ++c) {
		if(!shaders[c]) continue;

		shaders[c]->enable_vertex_attribute_with_pointer(
			Shader::get_input_name(Shader::Input::InputStart),
			2, GL_UNSIGNED_SHORT, GL_FALSE, SourceVertexSize,
			(void *)SourceVertexOffsetOfInputStart, 1);

		shaders[c]->enable_vertex_attribute_with_pointer(
			Shader::get_input_name(Shader::Input::OutputStart),
			2, GL_UNSIGNED_SHORT, GL_FALSE, SourceVertexSize,
			(void *)SourceVertexOffsetOfOutputStart, 1);

		shaders[c]->enable_vertex_attribute_with_pointer(
			Shader::get_input_name(Shader::Input::Ends),
			2, GL_UNSIGNED_SHORT, GL_FALSE, SourceVertexSize,
			(void *)SourceVertexOffsetOfEnds, 1);

		shaders[c]->enable_vertex_attribute_with_pointer(
			Shader::get_input_name(Shader::Input::PhaseTimeAndAmplitude),
			3, GL_UNSIGNED_BYTE, GL_FALSE, SourceVertexSize,
			(void *)SourceVertexOffsetOfPhaseTimeAndAmplitude, 1);
	}
}

void OpenGLOutputBuilder::prepare_output_shader() {
	output_shader_program_ = OpenGL::OutputShader::make_shader("", "texture(texID, srcCoordinatesVarying).rgb", false);
	output_shader_program_->set_source_texture_unit(work_texture_ ? work_texture_unit : filtered_texture_unit);
//	output_shader_program_->set_source_texture_unit(composite_texture_unit);
	output_shader_program_->set_origin_is_double_height(!!work_texture_);
}

void OpenGLOutputBuilder::prepare_output_vertex_array() {
	if(output_shader_program_) {
		glBindVertexArray(output_vertex_array_);
		array_builder.bind_output();
		
		using Shader = OpenGL::OutputShader;
		output_shader_program_->enable_vertex_attribute_with_pointer(
			Shader::get_input_name(Shader::Input::Horizontal),
			2, GL_UNSIGNED_SHORT, GL_FALSE, OutputVertexSize,
			(void *)OutputVertexOffsetOfHorizontal, 1);

		output_shader_program_->enable_vertex_attribute_with_pointer(
			Shader::get_input_name(Shader::Input::Vertical),
			2, GL_UNSIGNED_SHORT, GL_FALSE, OutputVertexSize,
			(void *)OutputVertexOffsetOfVertical, 1);
	}
}

// MARK: - Public Configuration

void OpenGLOutputBuilder::set_video_signal(VideoSignal video_signal) {
	if(video_signal_ != video_signal) {
		video_signal_ = video_signal;
		composite_src_output_y_ = 0;
		last_output_width_ = 0;
		last_output_height_ = 0;
		set_output_shader_width();
	}
}

void OpenGLOutputBuilder::set_timing(unsigned int input_frequency, unsigned int cycles_per_line, unsigned int height_of_display, unsigned int horizontal_scan_period, unsigned int vertical_scan_period, unsigned int vertical_period_divider) {
	std::lock_guard<std::mutex> lock_guard(output_mutex_);
	input_frequency_ = input_frequency;
	cycles_per_line_ = cycles_per_line;
	height_of_display_ = height_of_display;
	horizontal_scan_period_ = horizontal_scan_period;
	vertical_scan_period_ = vertical_scan_period;
	vertical_period_divider_ = vertical_period_divider;

	set_timing_uniforms();
}

// MARK: - Internal Configuration

void OpenGLOutputBuilder::set_colour_space_uniforms() {
	GLfloat rgbToYUV[] = {0.299f, -0.14713f, 0.615f, 0.587f, -0.28886f, -0.51499f, 0.114f, 0.436f, -0.10001f};
	GLfloat yuvToRGB[] = {1.0f, 1.0f, 1.0f, 0.0f, -0.39465f, 2.03211f, 1.13983f, -0.58060f, 0.0f};

	GLfloat rgbToYIQ[] = {0.299f, 0.596f, 0.211f, 0.587f, -0.274f, -0.523f, 0.114f, -0.322f, 0.312f};
	GLfloat yiqToRGB[] = {1.0f, 1.0f, 1.0f, 0.956f, -0.272f, -1.106f, 0.621f, -0.647f, 1.703f};

	GLfloat *fromRGB, *toRGB;

	switch(colour_space_) {
		case ColourSpace::YIQ:
			fromRGB = rgbToYIQ;
			toRGB = yiqToRGB;
		break;

		case ColourSpace::YUV:
			fromRGB = rgbToYUV;
			toRGB = yuvToRGB;
		break;

		default:	assert(false);	break;
	}

	if(composite_input_shader_program_)					composite_input_shader_program_->set_colour_conversion_matrices(fromRGB, toRGB);
	if(composite_separation_filter_program_)			composite_separation_filter_program_->set_colour_conversion_matrices(fromRGB, toRGB);
	if(composite_chrominance_filter_shader_program_)	composite_chrominance_filter_shader_program_->set_colour_conversion_matrices(fromRGB, toRGB);
	if(svideo_input_shader_program_)					svideo_input_shader_program_->set_colour_conversion_matrices(fromRGB, toRGB);
}

void OpenGLOutputBuilder::set_gamma() {
	if(output_shader_program_) output_shader_program_->set_gamma_ratio(gamma_);
}

/*!
	@returns The multiplier to apply to x positions received at the shader in order to produce locations in the intermediate
		texture. Intermediate textures are in phase with the composite signal, so this is a function of (i) composite frequency
		(determining how much of the texture adds up to a single line); and (ii) input frequency (determining what the input
		positions mean as a fraction of a line).
*/
float OpenGLOutputBuilder::get_composite_output_width() const {
	return
		(static_cast<float>(colour_cycle_numerator_ * 4) / static_cast<float>(colour_cycle_denominator_ * IntermediateBufferWidth)) *
		(static_cast<float>(IntermediateBufferWidth) / static_cast<float>(cycles_per_line_));
}

void OpenGLOutputBuilder::set_output_shader_width() {
	if(output_shader_program_) {
		// For anything that isn't RGB, scale so that sampling is in-phase with the colour subcarrier.
		const float width = (video_signal_ == VideoSignal::RGB) ? 1.0f : get_composite_output_width();
		output_shader_program_->set_input_width_scaler(width);
	}
}

void OpenGLOutputBuilder::set_timing_uniforms() {
	const float colour_subcarrier_frequency = static_cast<float>(colour_cycle_numerator_) / static_cast<float>(colour_cycle_denominator_);
	const float output_width = get_composite_output_width();
	const float sample_cycles_per_line = cycles_per_line_ / output_width;

	if(composite_separation_filter_program_) {
		composite_separation_filter_program_->set_width_scalers(output_width, output_width);
		composite_separation_filter_program_->set_separation_frequency(sample_cycles_per_line, colour_subcarrier_frequency);
		composite_separation_filter_program_->set_extension(6.0f);
	}
	if(composite_chrominance_filter_shader_program_) {
		composite_chrominance_filter_shader_program_->set_width_scalers(output_width, output_width);
		composite_chrominance_filter_shader_program_->set_extension(5.0f);
	}
	if(rgb_filter_shader_program_) {
		rgb_filter_shader_program_->set_width_scalers(1.0f, 1.0f);
		rgb_filter_shader_program_->set_filter_coefficients(sample_cycles_per_line, static_cast<float>(input_frequency_) * 0.5f);
	}
	if(output_shader_program_) {
		set_output_shader_width();
		output_shader_program_->set_timing(height_of_display_, cycles_per_line_, horizontal_scan_period_, vertical_scan_period_, vertical_period_divider_);
	}
	if(composite_input_shader_program_) {
		composite_input_shader_program_->set_width_scalers(1.0f, output_width);
		composite_input_shader_program_->set_extension(0.0f);
	}
	if(svideo_input_shader_program_) {
		svideo_input_shader_program_->set_width_scalers(1.0f, output_width);
		svideo_input_shader_program_->set_extension(0.0f);
	}
	if(rgb_input_shader_program_) {
		rgb_input_shader_program_->set_width_scalers(1.0f, 1.0f);
	}
}
