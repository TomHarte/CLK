//
//  ScanTarget.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/11/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#include "ScanTarget.hpp"

#include "OpenGL.hpp"

#include "Outputs/ScanTargets/FilterGenerator.hpp"
#include "Outputs/OpenGL/Shaders/CompositionShader.hpp"
#include "Outputs/OpenGL/Shaders/CopyShader.hpp"
#include "Outputs/OpenGL/Shaders/KernelShaders.hpp"
#include "Outputs/OpenGL/Shaders/LineOutputShader.hpp"

#include <algorithm>
#include <cstring>

using namespace Outputs::Display::OpenGL;

namespace {

/// The texture unit from which to source input data.
constexpr GLenum SourceDataTextureUnit = GL_TEXTURE0;

/// Contains the initial composition of scans into lines.
constexpr GLenum CompositionTextureUnit = GL_TEXTURE1;

/// If the input data was composite, contains separated  luma/chroma.
constexpr GLenum SeparationTextureUnit = GL_TEXTURE2;

/// If the input data was S-Video or composite, contains a fully demodulated image.
constexpr GLenum DemodulationTextureUnit = GL_TEXTURE3;

/// Contains the current display.
constexpr GLenum OutputTextureUnit = GL_TEXTURE4;

using Logger = Log::Logger<Log::Source::OpenGL>;

//constexpr GLint internalFormatForDepth(const std::size_t depth) {
//	switch(depth) {
//		default: return GL_FALSE;
//		case 1: return GL_R8UI;
//		case 2: return GL_RG8UI;
//		case 3: return GL_RGB8UI;
//		case 4: return GL_RGBA8UI;
//	}
//}
//
constexpr GLenum formatForDepth(const std::size_t depth) {
	switch(depth) {
		default: return GL_FALSE;
		case 1: return GL_RED_INTEGER;
		case 2: return GL_RG_INTEGER;
		case 3: return GL_RGB_INTEGER;
		case 4: return GL_RGBA_INTEGER;
	}
}

template <typename T> void allocate_buffer(
	const T &array,
	GLuint &buffer_name,
	GLuint &vertex_array_name
) {
	const auto buffer_size = array.size() * sizeof(array[0]);
	test_gl([&]{ glGenBuffers(1, &buffer_name); });
	test_gl([&]{ glBindBuffer(GL_ARRAY_BUFFER, buffer_name); });
	test_gl([&]{ glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(buffer_size), NULL, GL_STREAM_DRAW); });

	test_gl([&]{ glGenVertexArrays(1, &vertex_array_name); });
	test_gl([&]{ glBindVertexArray(vertex_array_name); });
	test_gl([&]{ glBindBuffer(GL_ARRAY_BUFFER, buffer_name); });
}

void fill_random(TextureTarget &target) {
	target.bind_texture();
	std::vector<uint8_t> image(target.width() * target.height() * 4);
	for(auto &c : image) {
		c = rand();
	}
	test_gl([&]{
		glTexImage2D(
			GL_TEXTURE_2D,
			0,
			GL_RGBA,
			target.width(),
			target.height(),
			0,
			GL_RGBA,
			GL_UNSIGNED_BYTE,
			image.data()
		);
	});
}
}

ScanTarget::ScanTarget(const API api, const GLuint target_framebuffer, const float output_gamma) :
	api_(api),
	target_framebuffer_(target_framebuffer),
	output_gamma_(output_gamma),
	full_display_rectangle_(api, -1.0f, -1.0f, 2.0f, 2.0f),
	scans_(scan_buffer_),
	lines_(line_buffer_),
	dirty_zones_(dirty_zones_buffer_) {

	set_scan_buffer(scan_buffer_.data(), scan_buffer_.size());
	set_line_buffer(line_buffer_.data(), line_metadata_buffer_.data(), line_buffer_.size());

	// TODO: if this is OpenGL 4.4 or newer, use glBufferStorage rather than glBufferData
	// and specify GL_MAP_PERSISTENT_BIT. Then map the buffer now, and let the client
	// write straight into it.

	test_gl([&]{ glBlendFunc(GL_SRC_ALPHA, GL_CONSTANT_COLOR); });
	test_gl([&]{ glBlendColor(0.4f, 0.4f, 0.4f, 1.0f); });

	// Establish initial state for is_drawing_to_accumulation_buffer_.
	is_drawing_to_output_.clear();
}

void ScanTarget::set_target_framebuffer(GLuint target_framebuffer) {
	perform([&] {
		target_framebuffer_ = target_framebuffer;
	});
}

void ScanTarget::setup_pipeline() {
	const auto modals = BufferingScanTarget::modals();
	const auto data_type_size = Outputs::Display::size_for_data_type(modals.input_data_type);

	// Possibly create a new source texture.
	if(source_texture_.empty() || source_texture_.channels() != data_type_size) {
		source_texture_ = Texture(
			data_type_size,
			SourceDataTextureUnit,
			GL_NEAREST,
			GL_NEAREST,
			WriteAreaWidth,
			WriteAreaHeight
		);
	}

	// Resize the texture if required.
	const size_t required_size = WriteAreaWidth*WriteAreaHeight*data_type_size;
	if(required_size != write_area_texture_.size()) {
		write_area_texture_.resize(required_size);
		set_write_area(write_area_texture_.data());
	}

	// Determine new sizing metrics.
	const auto buffer_width = FilterGenerator::SuggestedBufferWidth;
	const auto subcarrier_frequency = [](const Modals &modals) {
		return float(modals.colour_cycle_numerator) / float(modals.colour_cycle_denominator);
	};
	const float sample_multiplier =
		FilterGenerator::suggested_sample_multiplier(
			subcarrier_frequency(modals),
			modals.cycles_per_line
		);

	if(copy_shader_.empty()) {
		copy_shader_ = CopyShader(api_, {}, {});
	}

	if(composition_buffer_.empty()) {
		composition_buffer_ = TextureTarget(
			api_,
			buffer_width,
			LineBufferHeight,
			CompositionTextureUnit,
			GL_NEAREST,
			false
		);
	}

	if(
		!existing_modals_ ||
		existing_modals_->input_data_type != modals.input_data_type ||
		existing_modals_->display_type != modals.display_type ||
		existing_modals_->composite_colour_space != modals.composite_colour_space ||
		subcarrier_frequency(*existing_modals_) != subcarrier_frequency(modals)
	) {
		composition_shader_ = OpenGL::composition_shader(
			api_,
			modals.input_data_type,
			modals.display_type,
			modals.composite_colour_space,
			sample_multiplier,
			WriteAreaWidth, WriteAreaHeight,
			buffer_width, LineBufferHeight,
			scans_,
			GL_TEXTURE0
		);
	}

	if(
		!existing_modals_ ||
		modals.cycles_per_line != existing_modals_->cycles_per_line ||
		subcarrier_frequency(*existing_modals_) != subcarrier_frequency(modals)
	) {
		if(is_composite(modals.display_type)) {
			separation_shader_ = OpenGL::separation_shader(
				api_,
				subcarrier_frequency(modals),
				sample_multiplier * modals.cycles_per_line,
				buffer_width, LineBufferHeight,
				dirty_zones_,
				CompositionTextureUnit
			);
		} else {
			separation_shader_.reset();
		}

		if(is_composite(modals.display_type) || is_svideo(modals.display_type)) {
			demodulation_shader_ = OpenGL::demodulation_shader(
				api_,
				modals.composite_colour_space,
				modals.display_type,
				subcarrier_frequency(modals),
				sample_multiplier * modals.cycles_per_line,
				buffer_width,
				LineBufferHeight,
				dirty_zones_,
				is_svideo(modals.display_type) ? CompositionTextureUnit : SeparationTextureUnit
			);

			line_output_shader_ = OpenGL::line_output_shader(
				api_,
				buffer_width, LineBufferHeight,
				sample_multiplier,
				modals.expected_vertical_lines,
				modals.output_scale.x,
				modals.output_scale.y,
				lines_,
				DemodulationTextureUnit
			);

			fill_shader_ = OpenGL::FillShader(
				api_,
				sample_multiplier * modals.cycles_per_line,
				buffer_width,
				LineBufferHeight,
				dirty_zones_
			);
		} else {
			demodulation_shader_.reset();
			line_output_shader_.reset();
		}
	}

	if(
		!existing_modals_ ||
		modals.display_type != existing_modals_->display_type
	) {
		if(is_composite(modals.display_type)) {
			separation_buffer_ = TextureTarget(
				api_,
				buffer_width,
				LineBufferHeight,
				SeparationTextureUnit,
				GL_NEAREST,
				false
			);
		} else {
			separation_buffer_.reset();
		}

		if(is_composite(modals.display_type) || is_svideo(modals.display_type)) {
			demodulation_buffer_ = TextureTarget(
				api_,
				buffer_width,
				LineBufferHeight,
				DemodulationTextureUnit,
				GL_LINEAR,
				false
			);
		} else {
			demodulation_buffer_.reset();
		}
	}

	existing_modals_ = modals;
}

bool ScanTarget::is_soft_display_type() {
	const auto display_type = modals().display_type;
	return display_type == DisplayType::CompositeColour || display_type == DisplayType::CompositeMonochrome;
}

void ScanTarget::update(const int output_width, const int output_height) {
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
	perform([&] {
		const OutputArea area = get_output_area();

		// Establish the pipeline if necessary.
		const auto new_modals = BufferingScanTarget::new_modals();
		if(bool(new_modals)) {
			setup_pipeline();
		}

		// Determine the start time of this submission group and the number of lines it will contain.
		line_submission_begin_time_ = std::chrono::high_resolution_clock::now();
		lines_submitted_ = (area.end.line - area.begin.line + line_buffer_.size()) % line_buffer_.size();

		// Submit texture.
		if(area.begin.write_area_x != area.end.write_area_x || area.begin.write_area_y != area.end.write_area_y) {
			source_texture_.bind();

			const auto submit = [&](const GLint y_begin, const GLint y_end) {
				test_gl([&]{
					glTexSubImage2D(
						GL_TEXTURE_2D, 0,
						0, y_begin,
						WriteAreaWidth,
						y_end - y_begin,
						formatForDepth(write_area_data_size()),
						GL_UNSIGNED_BYTE,
						&write_area_texture_[size_t(y_begin * WriteAreaWidth) * source_texture_.channels()]
					);
				});
			};

			// Both of the following upload to area.end.write_area_y + 1 to include whatever line the write area
			// is currently on. It may have partial source areas along it, despite being incomplete.
			if(area.end.write_area_y >= area.begin.write_area_y) {
				// Submit the direct region from the submit pointer to the read pointer.
				submit(area.begin.write_area_y, area.end.write_area_y + 1);
			} else {
				// The circular buffer wrapped around; submit the data from the read pointer to the end of
				// the buffer and from the start of the buffer to the submit pointer.
				submit(area.begin.write_area_y, WriteAreaHeight);
				submit(0, area.end.write_area_y + 1);
			}
		}

		// Submit scans; only the new ones need to be communicated.
		if(area.end.scan != area.begin.scan) {
			const size_t new_scans = (area.end.scan - area.begin.scan + scan_buffer_.size()) % scan_buffer_.size();

			// Submit new scans.
			// First implementation: put all new scans at the start of the buffer, for a simple
			// glDrawArraysInstanced call below.
			scans_.bind_buffer();
			size_t buffer_destination = 0;
			const auto submit = [&](const size_t begin, const size_t end) {
				test_gl([&]{ 
					glBufferSubData(
						GL_ARRAY_BUFFER,
						buffer_destination,
						(end - begin) * sizeof(Scan),
						&scan_buffer_[begin]
					);
				});
				buffer_destination += (end - begin) * sizeof(Scan);
			};
			if(area.begin.scan < area.end.scan) {
				submit(area.begin.scan, area.end.scan);
			} else {
				submit(area.begin.scan, scan_buffer_.size());
				submit(0, area.end.scan);
			}

			// Populate composition buffer.
			composition_buffer_.bind_framebuffer();
			scans_.bind();
			composition_shader_.bind();
			test_gl([&]{ glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, GLsizei(new_scans)); });
		}

		// Work with the accumulation_buffer_ potentially starts from here onwards; set its flag.
		while(is_drawing_to_output_.test_and_set());

		// Make sure there's an appropriately-sized buffer.
		const auto output_buffer_width = output_width * 2;
		const auto output_buffer_height = output_height * 2;
		if(
			output_buffer_.empty() ||
			output_buffer_.width() != output_buffer_width ||
			output_buffer_.height() != output_buffer_height
		) {
			output_buffer_ = TextureTarget(
				api_,
				output_buffer_width,
				output_buffer_height,
				OutputTextureUnit,
				GL_NEAREST,
				false	// TODO: should probably be true, if I'm going to use stencil (?)
			);
		}

		// Figure out how many new lines are ready.
		if(area.end.line != area.begin.line) {
			const auto new_lines = (area.end.line - area.begin.line + LineBufferHeight) % LineBufferHeight;

			// Populate dirty zones, and record quantity.
			const int num_dirty_zones = 1 + (area.begin.line >= area.end.line);
			dirty_zones_buffer_[0].begin = area.begin.line;
			if(num_dirty_zones == 1) {
				dirty_zones_buffer_[0].end = area.end.line;
			} else {
				dirty_zones_buffer_[0].end = LineBufferHeight;
				dirty_zones_buffer_[1].begin = 0;
				dirty_zones_buffer_[1].end = area.end.line;
			}

			dirty_zones_.bind_all();
			test_gl([&]{
				glBufferSubData(
					GL_ARRAY_BUFFER,
					0,
					num_dirty_zones * sizeof(DirtyZone),
					dirty_zones_buffer_.data()
				);
			});

			// Perform [composite/svideo] -> RGB conversion.
			if(is_composite(existing_modals_->display_type)) {
				separation_buffer_.bind_framebuffer();
				separation_shader_.bind();
				test_gl([&]{ glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, GLsizei(num_dirty_zones)); });
			}

			if(is_composite(existing_modals_->display_type) || is_svideo(existing_modals_->display_type)) {
				demodulation_buffer_.bind_framebuffer();
				demodulation_shader_.bind();
				test_gl([&]{ glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, GLsizei(num_dirty_zones)); });
			}

			// Now retroactively clear the composition buffer; doing this post hoc avoids uncertainty about the
			// exact timing of a new line being drawn to, as well as fitting more neatly into when dirty zones
			// are bound.
			composition_buffer_.bind_framebuffer();
			if(is_composite(existing_modals_->display_type)) {
				fill_shader_.bind(0.0, 0.0, 0.0, 0.0);
			} else {
				fill_shader_.bind(0.0, 0.5, 0.5, 1.0);
			}
			test_gl([&]{ glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, GLsizei(num_dirty_zones)); });

			// Submit new lines.
			lines_.bind_all();
			size_t buffer_destination = 0;
			const auto submit = [&](const size_t begin, const size_t end) {
				const auto size = (end - begin) * sizeof(Line);
				test_gl([&]{
					glBufferSubData(
						GL_ARRAY_BUFFER,
						buffer_destination,
						size,
						&line_buffer_[begin]
					);
				});
				buffer_destination += (end - begin) * sizeof(Line);
			};
			if(area.begin.line < area.end.line) {
				submit(area.begin.line, area.end.line);
			} else {
				submit(area.begin.line, line_buffer_.size());
				submit(0, area.end.line);
			}

			// Output new lines.
			line_output_shader_.bind();
			output_buffer_.bind_framebuffer();
			test_gl([&]{ glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, GLsizei(new_lines)); });

			// TODO: end-of-frame blanking of untouched areas.

		}

		// That's it for operations affecting the accumulation buffer.
		is_drawing_to_output_.clear();

		// Grab a fence sync object to avoid busy waiting upon the next extry into this
		// function, and reset the is_updating_ flag.
		fence_ = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
		complete_output_area(area);
	});
}

void ScanTarget::draw(int output_width, int output_height) {
	while(is_drawing_to_output_.test_and_set(std::memory_order_acquire));

	if(!composition_buffer_.empty()) {
		// Copy the accumulation texture to the target.
		test_gl([&]{ glBindFramebuffer(GL_FRAMEBUFFER, target_framebuffer_); });
		test_gl([&]{ glViewport(0, 0, (GLsizei)output_width, (GLsizei)output_height); });
		copy_shader_.perform(OutputTextureUnit);
	}

	is_drawing_to_output_.clear(std::memory_order_release);
}
