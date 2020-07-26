//
//  ScanTarget.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/11/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#include "ScanTarget.hpp"

#include "OpenGL.hpp"
#include "Primitives/Rectangle.hpp"

#include <cassert>
#include <cstring>
#include <limits>

using namespace Outputs::Display::OpenGL;

#ifndef NDEBUG
//#define LOG_LINES
//#define LOG_SCANS
#endif

namespace {

/// The texture unit from which to source input data.
constexpr GLenum SourceDataTextureUnit = GL_TEXTURE0;

/// The texture unit which contains raw line-by-line composite, S-Video or RGB data.
constexpr GLenum UnprocessedLineBufferTextureUnit = GL_TEXTURE1;

/// The texture unit that contains a pre-lowpass-filtered but fixed-resolution version of the chroma signal;
/// this is used when processing composite video only, and for chroma information only. Luminance is calculated
/// at the fidelity permitted by the output target, but my efforts to separate, demodulate and filter
/// chrominance during output without either massively sampling or else incurring significant high-frequency
/// noise that sampling reduces into a Moire, have proven to be unsuccessful for the time being.
constexpr GLenum QAMChromaTextureUnit = GL_TEXTURE2;

/// The texture unit that contains the current display.
constexpr GLenum AccumulationTextureUnit = GL_TEXTURE3;

constexpr GLint internalFormatForDepth(std::size_t depth) {
	switch(depth) {
		default: return GL_FALSE;
		case 1: return GL_R8UI;
		case 2: return GL_RG8UI;
		case 3: return GL_RGB8UI;
		case 4: return GL_RGBA8UI;
	}
}

constexpr GLenum formatForDepth(std::size_t depth) {
	switch(depth) {
		default: return GL_FALSE;
		case 1: return GL_RED_INTEGER;
		case 2: return GL_RG_INTEGER;
		case 3: return GL_RGB_INTEGER;
		case 4: return GL_RGBA_INTEGER;
	}
}

}

template <typename T> void ScanTarget::allocate_buffer(const T &array, GLuint &buffer_name, GLuint &vertex_array_name) {
	const auto buffer_size = array.size() * sizeof(array[0]);
	test_gl(glGenBuffers, 1, &buffer_name);
	test_gl(glBindBuffer, GL_ARRAY_BUFFER, buffer_name);
	test_gl(glBufferData, GL_ARRAY_BUFFER, GLsizeiptr(buffer_size), NULL, GL_STREAM_DRAW);

	test_gl(glGenVertexArrays, 1, &vertex_array_name);
	test_gl(glBindVertexArray, vertex_array_name);
	test_gl(glBindBuffer, GL_ARRAY_BUFFER, buffer_name);
}

ScanTarget::ScanTarget(GLuint target_framebuffer, float output_gamma) :
	target_framebuffer_(target_framebuffer),
	output_gamma_(output_gamma),
	unprocessed_line_texture_(LineBufferWidth, LineBufferHeight, UnprocessedLineBufferTextureUnit, GL_NEAREST, false),
	full_display_rectangle_(-1.0f, -1.0f, 2.0f, 2.0f) {

	// Allocate space for the scans and lines.
	allocate_buffer(scan_buffer_, scan_buffer_name_, scan_vertex_array_);
	allocate_buffer(line_buffer_, line_buffer_name_, line_vertex_array_);

	// TODO: if this is OpenGL 4.4 or newer, use glBufferStorage rather than glBufferData
	// and specify GL_MAP_PERSISTENT_BIT. Then map the buffer now, and let the client
	// write straight into it.

	test_gl(glGenTextures, 1, &write_area_texture_name_);

	test_gl(glBlendFunc, GL_SRC_ALPHA, GL_CONSTANT_COLOR);
	test_gl(glBlendColor, 0.4f, 0.4f, 0.4f, 1.0f);

	// Establish initial state for is_drawing_to_accumulation_buffer_.
	is_drawing_to_accumulation_buffer_.clear();
}

ScanTarget::~ScanTarget() {
	perform([=] {
		glDeleteBuffers(1, &scan_buffer_name_);
		glDeleteTextures(1, &write_area_texture_name_);
		glDeleteVertexArrays(1, &scan_vertex_array_);
	});
}

void ScanTarget::set_target_framebuffer(GLuint target_framebuffer) {
	perform([=] {
		target_framebuffer_ = target_framebuffer;
	});
}

void ScanTarget::setup_pipeline() {
	const auto data_type_size = Outputs::Display::size_for_data_type(modals_.input_data_type);

	// Resize the texture only if required.
	if(data_type_size != write_area_data_size()) {
		write_area_texture_.resize(WriteAreaWidth*WriteAreaHeight*data_type_size);
		set_write_area(write_area_texture_.data());
	}

	// Prepare to bind line shaders.
	test_gl(glBindVertexArray, line_vertex_array_);
	test_gl(glBindBuffer, GL_ARRAY_BUFFER, line_buffer_name_);

	// Destroy or create a QAM buffer and shader, if appropriate.
	const bool needs_qam_buffer = (modals_.display_type == DisplayType::CompositeColour || modals_.display_type == DisplayType::SVideo);
	if(needs_qam_buffer) {
		if(!qam_chroma_texture_) {
			qam_chroma_texture_ = std::make_unique<TextureTarget>(LineBufferWidth, LineBufferHeight, QAMChromaTextureUnit, GL_NEAREST, false);
		}

		qam_separation_shader_ = qam_separation_shader();
		enable_vertex_attributes(ShaderType::QAMSeparation, *qam_separation_shader_);
		set_uniforms(ShaderType::QAMSeparation, *qam_separation_shader_);
		qam_separation_shader_->set_uniform("textureName", GLint(UnprocessedLineBufferTextureUnit - GL_TEXTURE0));
	} else {
		qam_chroma_texture_.reset();
		qam_separation_shader_.reset();
	}

	// Establish an output shader.
	output_shader_ = conversion_shader();
	enable_vertex_attributes(ShaderType::Conversion, *output_shader_);
	set_uniforms(ShaderType::Conversion, *output_shader_);
	output_shader_->set_uniform("origin", modals_.visible_area.origin.x, modals_.visible_area.origin.y);
	output_shader_->set_uniform("size", modals_.visible_area.size.width, modals_.visible_area.size.height);
	output_shader_->set_uniform("textureName", GLint(UnprocessedLineBufferTextureUnit - GL_TEXTURE0));
	output_shader_->set_uniform("qamTextureName", GLint(QAMChromaTextureUnit - GL_TEXTURE0));

	// Establish an input shader.
	input_shader_ = composition_shader();
	test_gl(glBindVertexArray, scan_vertex_array_);
	test_gl(glBindBuffer, GL_ARRAY_BUFFER, scan_buffer_name_);
	enable_vertex_attributes(ShaderType::Composition, *input_shader_);
	set_uniforms(ShaderType::Composition, *input_shader_);
	input_shader_->set_uniform("textureName", GLint(SourceDataTextureUnit - GL_TEXTURE0));
}

bool ScanTarget::is_soft_display_type() {
	return modals_.display_type == DisplayType::CompositeColour || modals_.display_type == DisplayType::CompositeMonochrome;
}

void ScanTarget::update(int, int output_height) {
	// If the GPU is still busy, don't wait; we'll catch it next time.
	if(fence_ != nullptr) {
		if(glClientWaitSync(fence_, GL_SYNC_FLUSH_COMMANDS_BIT, 0) == GL_TIMEOUT_EXPIRED) {
			display_metrics_.announce_draw_status(
				lines_submitted_,
				std::chrono::high_resolution_clock::now() - line_submission_begin_time_,
				false);
			return;
		}
		fence_ = nullptr;
	}

	// Update the display metrics.
	display_metrics_.announce_draw_status(
		lines_submitted_,
		std::chrono::high_resolution_clock::now() - line_submission_begin_time_,
		true);

	// Grab the new output list.
	perform([=] (const OutputArea &area) {
		// Establish the pipeline if necessary.
		const bool did_setup_pipeline = modals_are_dirty_;
		if(modals_are_dirty_) {
			setup_pipeline();
			modals_are_dirty_ = false;
		}

		// Determine the start time of this submission group and the number of lines it will contain.
		line_submission_begin_time_ = std::chrono::high_resolution_clock::now();
		lines_submitted_ = (area.end.line - area.start.line + line_buffer_.size()) % line_buffer_.size();

		// Submit scans; only the new ones need to be communicated.
		size_t new_scans = (area.end.scan - area.start.scan + scan_buffer_.size()) % scan_buffer_.size();
		if(new_scans) {
			test_gl(glBindBuffer, GL_ARRAY_BUFFER, scan_buffer_name_);

			// Map only the required portion of the buffer.
			const size_t new_scans_size = new_scans * sizeof(Scan);
			uint8_t *const destination = static_cast<uint8_t *>(
				glMapBufferRange(GL_ARRAY_BUFFER, 0, GLsizeiptr(new_scans_size), GL_MAP_WRITE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT)
			);
			test_gl_error();

			// Copy as a single chunk if possible; otherwise copy in two parts.
			if(area.start.scan < area.end.scan) {
				memcpy(destination, &scan_buffer_[size_t(area.start.scan)], new_scans_size);
			} else {
				const size_t first_portion_length = (scan_buffer_.size() - area.start.scan) * sizeof(Scan);
				memcpy(destination, &scan_buffer_[area.start.scan], first_portion_length);
				memcpy(&destination[first_portion_length], &scan_buffer_[0], new_scans_size - first_portion_length);
			}

			// Flush and unmap the buffer.
			test_gl(glFlushMappedBufferRange, GL_ARRAY_BUFFER, 0, GLsizeiptr(new_scans_size));
			test_gl(glUnmapBuffer, GL_ARRAY_BUFFER);
		}

		// Submit texture.
		if(area.start.write_area_x != area.end.write_area_x || area.start.write_area_y != area.end.write_area_y) {
			test_gl(glActiveTexture, SourceDataTextureUnit);
			test_gl(glBindTexture, GL_TEXTURE_2D, write_area_texture_name_);

			// Create storage for the texture if it doesn't yet exist; this was deferred until here
			// because the pixel format wasn't initially known.
			if(!texture_exists_) {
				test_gl(glTexParameteri, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				test_gl(glTexParameteri, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				test_gl(glTexParameteri, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
				test_gl(glTexParameteri, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				test_gl(glTexImage2D,
					GL_TEXTURE_2D,
					0,
					internalFormatForDepth(write_area_data_size()),
					WriteAreaWidth,
					WriteAreaHeight,
					0,
					formatForDepth(write_area_data_size()),
					GL_UNSIGNED_BYTE,
					nullptr);
				texture_exists_ = true;
			}

			if(area.end.write_area_y >= area.start.write_area_y) {
				// Submit the direct region from the submit pointer to the read pointer.
				test_gl(glTexSubImage2D,
					GL_TEXTURE_2D, 0,
					0, area.start.write_area_y,
					WriteAreaWidth,
					1 + area.end.write_area_y - area.start.write_area_y,
					formatForDepth(write_area_data_size()),
					GL_UNSIGNED_BYTE,
					&write_area_texture_[size_t(TextureAddress(0, area.start.write_area_y)) * write_area_data_size()]);
			} else {
				// The circular buffer wrapped around; submit the data from the read pointer to the end of
				// the buffer and from the start of the buffer to the submit pointer.
				test_gl(glTexSubImage2D,
					GL_TEXTURE_2D, 0,
					0, area.start.write_area_y,
					WriteAreaWidth,
					WriteAreaHeight - area.start.write_area_y,
					formatForDepth(write_area_data_size()),
					GL_UNSIGNED_BYTE,
					&write_area_texture_[size_t(TextureAddress(0, area.start.write_area_y)) * write_area_data_size()]);
				test_gl(glTexSubImage2D,
					GL_TEXTURE_2D, 0,
					0, 0,
					WriteAreaWidth,
					1 + area.end.write_area_y,
					formatForDepth(write_area_data_size()),
					GL_UNSIGNED_BYTE,
					&write_area_texture_[0]);
			}
		}

		// Push new input to the unprocessed line buffer.
		if(new_scans) {
			unprocessed_line_texture_.bind_framebuffer();

			// Clear newly-touched lines; that is everything from (read+1) to submit.
			const auto first_line_to_clear = GLsizei((area.start.line+1)%line_buffer_.size());
			const auto final_line_to_clear = GLsizei(area.end.line);
			if(first_line_to_clear != final_line_to_clear) {
				test_gl(glEnable, GL_SCISSOR_TEST);

				// Determine the proper clear colour — this needs to be anything that describes black
				// in the input colour encoding at use.
				if(modals_.input_data_type == InputDataType::Luminance8Phase8) {
					// Supply both a zero luminance and a colour-subcarrier-disengaging phase.
					test_gl(glClearColor, 0.0f, 1.0f, 0.0f, 0.0f);
				} else {
					test_gl(glClearColor, 0.0f, 0.0f, 0.0f, 0.0f);
				}

				if(first_line_to_clear < final_line_to_clear) {
					test_gl(glScissor, GLint(0), GLint(first_line_to_clear), unprocessed_line_texture_.get_width(), final_line_to_clear - first_line_to_clear);
					test_gl(glClear, GL_COLOR_BUFFER_BIT);
				} else {
					test_gl(glScissor, GLint(0), GLint(0), unprocessed_line_texture_.get_width(), final_line_to_clear);
					test_gl(glClear, GL_COLOR_BUFFER_BIT);
					test_gl(glScissor, GLint(0), GLint(first_line_to_clear), unprocessed_line_texture_.get_width(), unprocessed_line_texture_.get_height() - first_line_to_clear);
					test_gl(glClear, GL_COLOR_BUFFER_BIT);
				}

				test_gl(glDisable, GL_SCISSOR_TEST);
			}

			// Apply new spans. They definitely always go to the first buffer.
			test_gl(glBindVertexArray, scan_vertex_array_);
			input_shader_->bind();
			test_gl(glDrawArraysInstanced, GL_TRIANGLE_STRIP, 0, 4, GLsizei(new_scans));
		}

		// Logic for reducing resolution: start doing so if the metrics object reports that
		// it's a good idea. Go up to a quarter of the requested resolution, subject to
		// clamping at each stage. If the output resolution changes, or anything else about
		// the output pipeline, just start trying the highest size again.
		if(display_metrics_.should_lower_resolution() && is_soft_display_type()) {
			resolution_reduction_level_ = std::min(resolution_reduction_level_+1, 4);
		}
		if(output_height_ != output_height || did_setup_pipeline) {
			resolution_reduction_level_ = 1;
			output_height_ = output_height;
		}

		// Ensure the accumulation buffer is properly sized, allowing for the metrics object's
		// feelings about whether too high a resolution is being used.
		const int framebuffer_height = std::max(output_height / resolution_reduction_level_, std::min(540, output_height));
		const int proportional_width = (framebuffer_height * 4) / 3;
		const bool did_create_accumulation_texture = !accumulation_texture_ || ( (accumulation_texture_->get_width() != proportional_width || accumulation_texture_->get_height() != framebuffer_height));

		// Work with the accumulation_buffer_ potentially starts from here onwards; set its flag.
		while(is_drawing_to_accumulation_buffer_.test_and_set());
		if(did_create_accumulation_texture) {
			LOG("Changed output resolution to " << proportional_width << " by " << framebuffer_height);
			display_metrics_.announce_did_resize();
			std::unique_ptr<OpenGL::TextureTarget> new_framebuffer(
				new TextureTarget(
					GLsizei(proportional_width),
					GLsizei(framebuffer_height),
					AccumulationTextureUnit,
					GL_NEAREST,
					true));
			if(accumulation_texture_) {
				new_framebuffer->bind_framebuffer();
				test_gl(glClear, GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

				test_gl(glActiveTexture, AccumulationTextureUnit);
				accumulation_texture_->bind_texture();
				accumulation_texture_->draw(4.0f / 3.0f);

				test_gl(glClear, GL_STENCIL_BUFFER_BIT);

				new_framebuffer->bind_texture();
			}
			accumulation_texture_ = std::move(new_framebuffer);

			// In the absence of a way to resize a stencil buffer, just mark
			// what's currently present as invalid to avoid an improper clear
			// for this frame.
			stencil_is_valid_ = false;
		}

		if(did_setup_pipeline || did_create_accumulation_texture) {
			set_sampling_window(proportional_width, framebuffer_height, *output_shader_);
		}

		// Figure out how many new lines are ready.
		auto new_lines = (area.end.line - area.start.line + LineBufferHeight) % LineBufferHeight;
		if(new_lines) {
			// Prepare to output lines.
			test_gl(glBindVertexArray, line_vertex_array_);

			// Bind the accumulation framebuffer, unless there's going to be QAM work first.
			if(!qam_separation_shader_ || line_metadata_buffer_[area.start.line].is_first_in_frame) {
				accumulation_texture_->bind_framebuffer();
				output_shader_->bind();

				// Enable blending and stenciling.
				test_gl(glEnable, GL_BLEND);
				test_gl(glEnable, GL_STENCIL_TEST);
			}

			// Set the proper stencil function regardless.
			test_gl(glStencilFunc, GL_EQUAL, 0, GLuint(~0));
			test_gl(glStencilOp, GL_KEEP, GL_KEEP, GL_INCR);

			// Prepare to upload data that will consitute lines.
			test_gl(glBindBuffer, GL_ARRAY_BUFFER, line_buffer_name_);

			// Divide spans by which frame they're in.
			auto start_line = area.start.line;
			while(new_lines) {
				uint16_t end_line = (start_line + 1) % LineBufferHeight;

				// Find the limit of spans to draw in this cycle.
				size_t lines = 1;
				while(end_line != area.end.line && !line_metadata_buffer_[end_line].is_first_in_frame) {
					end_line = (end_line + 1) % LineBufferHeight;
					++lines;
				}

				// If this is start-of-frame, clear any untouched pixels and flush the stencil buffer
				if(line_metadata_buffer_[start_line].is_first_in_frame) {
					if(stencil_is_valid_ && line_metadata_buffer_[start_line].previous_frame_was_complete) {
						full_display_rectangle_.draw(0.0f, 0.0f, 0.0f);
					}
					stencil_is_valid_ = true;
					test_gl(glClear, GL_STENCIL_BUFFER_BIT);

					// Rebind the program for span output.
					test_gl(glBindVertexArray, line_vertex_array_);
					if(!qam_separation_shader_) {
						output_shader_->bind();
					}
				}

				// Upload.
				const auto buffer_size = lines * sizeof(Line);
				if(!end_line || end_line > start_line) {
					test_gl(glBufferSubData, GL_ARRAY_BUFFER, 0, GLsizeiptr(buffer_size), &line_buffer_[start_line]);
				} else {
					uint8_t *destination = static_cast<uint8_t *>(
						glMapBufferRange(GL_ARRAY_BUFFER, 0, GLsizeiptr(buffer_size), GL_MAP_WRITE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT)
					);
					assert(destination);
					test_gl_error();

					const size_t buffer_length = line_buffer_.size() * sizeof(Line);
					const size_t start_position = start_line * sizeof(Line);
					memcpy(&destination[0], &line_buffer_[start_line], buffer_length - start_position);
					memcpy(&destination[buffer_length - start_position], &line_buffer_[0], end_line * sizeof(Line));

					test_gl(glFlushMappedBufferRange, GL_ARRAY_BUFFER, 0, GLsizeiptr(buffer_size));
					test_gl(glUnmapBuffer, GL_ARRAY_BUFFER);
				}

				// Produce colour information, if required.
				if(qam_separation_shader_) {
					qam_separation_shader_->bind();
					qam_chroma_texture_->bind_framebuffer();
					test_gl(glClear, GL_COLOR_BUFFER_BIT);	// TODO: this is here as a hint that the old framebuffer doesn't need reloading;
															// test whether that's a valid optimisation on desktop OpenGL.

					test_gl(glDisable, GL_BLEND);
					test_gl(glDisable, GL_STENCIL_TEST);
					test_gl(glDrawArraysInstanced, GL_TRIANGLE_STRIP, 0, 4, GLsizei(lines));

					accumulation_texture_->bind_framebuffer();
					output_shader_->bind();
					test_gl(glEnable, GL_BLEND);
					test_gl(glEnable, GL_STENCIL_TEST);
				}

				// Render to the output.
				test_gl(glDrawArraysInstanced, GL_TRIANGLE_STRIP, 0, 4, GLsizei(lines));

				start_line = end_line;
				new_lines -= lines;
			}

			// Disable blending and the stencil test again.
			test_gl(glDisable, GL_STENCIL_TEST);
			test_gl(glDisable, GL_BLEND);
		}

		// That's it for operations affecting the accumulation buffer.
		is_drawing_to_accumulation_buffer_.clear();

		// Grab a fence sync object to avoid busy waiting upon the next extry into this
		// function, and reset the is_updating_ flag.
		fence_ = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
	});
}

void ScanTarget::draw(int output_width, int output_height) {
	while(is_drawing_to_accumulation_buffer_.test_and_set(std::memory_order_acquire));

	if(accumulation_texture_) {
		// Copy the accumulation texture to the target.
		test_gl(glBindFramebuffer, GL_FRAMEBUFFER, target_framebuffer_);
		test_gl(glViewport, 0, 0, (GLsizei)output_width, (GLsizei)output_height);

		test_gl(glClearColor, 0.0f, 0.0f, 0.0f, 0.0f);
		test_gl(glClear, GL_COLOR_BUFFER_BIT);
		accumulation_texture_->bind_texture();
		accumulation_texture_->draw(float(output_width) / float(output_height), 4.0f / 255.0f);
	}

	is_drawing_to_accumulation_buffer_.clear(std::memory_order_release);
}
