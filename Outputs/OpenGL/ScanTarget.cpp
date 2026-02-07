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

template <typename SourceT>
size_t submit(VertexArray &target, const size_t begin, const size_t end, const SourceT &source) {
	if(begin == end) {
		return 0;
	}

	target.bind_buffer();
	size_t buffer_destination = 0;
	const auto submit = [&](const size_t begin, const size_t end) {
		test_gl([&]{
			glBufferSubData(
				GL_ARRAY_BUFFER,
				buffer_destination,
				(end - begin) * sizeof(source[0]),
				&source[begin]
			);
		});
		buffer_destination += (end - begin) * sizeof(source[0]);
	};
	if(begin < end) {
		submit(begin, end);
		return end - begin;
	} else {
		submit(begin, source.size());
		submit(0, end);
		return source.size() - begin + end;
	}
}

size_t distance(const size_t begin, const size_t end, const size_t buffer_length) {
	return end >= begin ? end - begin : buffer_length + end - begin;
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

	// Set stencil function for underdraw.
	test_gl([&]{ glStencilFunc(GL_EQUAL, 0, GLuint(~0)); });
	test_gl([&]{ glStencilOp(GL_KEEP, GL_KEEP, GL_INCR); });

	// Establish initial state for is_drawing_to_accumulation_buffer_.
	is_drawing_to_output_.clear();
}

void ScanTarget::set_target_framebuffer(const GLuint target_framebuffer) {
	perform([&] {
		target_framebuffer_ = target_framebuffer;
	});
}

void ScanTarget::update_aspect_ratio_transformation() {
	if(output_buffer_.empty()) {
		return;
	}

	const auto framing = aspect_ratio_transformation(
		BufferingScanTarget::modals(),
		float(output_buffer_.width()) / float(output_buffer_.height())
	);

	if(!line_output_shader_.empty()) {
		line_output_shader_.set_aspect_ratio_transformation(framing);
	}
	if(!scan_output_shader_.empty()) {
		scan_output_shader_.set_aspect_ratio_transformation(framing);
	}
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

	if(
		copy_shader_.empty() ||
		!existing_modals_ ||
		existing_modals_->brightness != modals.brightness ||
		existing_modals_->intended_gamma != modals.intended_gamma
	) {
		copy_shader_ = CopyShader(
			api_,
			modals.brightness != 1.0f ? std::optional<float>(modals.brightness) : std::optional<float>(),
			modals.intended_gamma != output_gamma_ ?
				std::optional<float>(output_gamma_ / modals.intended_gamma) :
				std::optional<float>()
		);
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

	if(is_rgb(modals.display_type)) {
		composition_shader_.reset();
		separation_shader_.reset();
		demodulation_shader_.reset();
		line_output_shader_.reset();

		if(
			scan_output_shader_.empty() ||
			existing_modals_->input_data_type != modals.input_data_type ||
			existing_modals_->expected_vertical_lines != modals.expected_vertical_lines ||
			existing_modals_->output_scale.x != modals.output_scale.x ||
			existing_modals_->output_scale.y != modals.output_scale.y
		) {
			scan_output_shader_ = OpenGL::ScanOutputShader(
				api_,
				modals.input_data_type,
				modals.expected_vertical_lines,
				modals.output_scale.x,
				modals.output_scale.y,
				WriteAreaWidth,
				WriteAreaHeight,
				scans_,
				SourceDataTextureUnit);
		}
	} else {
		scan_output_shader_.reset();

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

				line_output_shader_ = LineOutputShader(
					api_,
					buffer_width, LineBufferHeight,
					sample_multiplier,
					modals.expected_vertical_lines,
					modals.output_scale.x,
					modals.output_scale.y,
					0.64f,
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
	}

	update_aspect_ratio_transformation();
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
		lines_submitted_ = distance(area.begin.line, area.end.line, line_buffer_.size());

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
						source_texture_.format(),
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

		if(!is_rgb(existing_modals_->display_type)) {
			process_to_rgb(area);
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
			// TODO: resize old output buffer into new.
			output_buffer_ = TextureTarget(
				api_,
				output_buffer_width,
				output_buffer_height,
				OutputTextureUnit,
				GL_NEAREST,
				true
			);
			update_aspect_ratio_transformation();
		}

		output_buffer_.bind_framebuffer();
		test_gl([&]{ glEnable(GL_BLEND); });
		test_gl([&]{ glEnable(GL_STENCIL_TEST); });

		if(!is_rgb(existing_modals_->display_type)) {
			output_lines(area);
		} else {
			output_scans(area);
		}

		test_gl([&]{ glDisable(GL_BLEND); });
		test_gl([&]{ glDisable(GL_STENCIL_TEST); });

		// That's it for operations affecting the accumulation buffer.
		is_drawing_to_output_.clear();

		// Grab a fence sync object to avoid busy waiting upon the next extry into this
		// function, and reset the is_updating_ flag.
		fence_ = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
		complete_output_area(area);
	});
}

void ScanTarget::process_to_rgb(const OutputArea &area) {
	if(area.end.scan != area.begin.scan) {
		// Submit all scans.
		const auto new_scans = ::submit(scans_, area.begin.scan, area.end.scan, scan_buffer_);

		// Populate composition buffer.
		composition_buffer_.bind_framebuffer();
		scans_.bind();
		composition_shader_.bind();
		test_gl([&]{ glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, GLsizei(new_scans)); });
	}

	// Do S-Video or composite line decoding.
	if(area.end.line != area.begin.line) {
		// Calculate and submit line dirty zones.
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

		// Retroactively clear the composition buffer; doing this post hoc avoids uncertainty about the
		// exact timing of a new line being drawn to, as well as fitting more neatly into when dirty zones
		// are bound.
		composition_buffer_.bind_framebuffer();
		if(is_composite(existing_modals_->display_type)) {
			fill_shader_.bind(0.0, 0.0, 0.0, 0.0);
		} else {
			fill_shader_.bind(0.0, 0.5, 0.5, 1.0);
		}
		test_gl([&]{ glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, GLsizei(num_dirty_zones)); });
	}
}

void ScanTarget::output_lines(const OutputArea &area) {
	if(area.end.line == area.begin.line) {
		return;
	}

	// Batch lines by frame.
	size_t begin = area.begin.line;
	while(begin != area.end.line) {
		// Apply end-of-frame cleaning if necessary.
		if(line_metadata_buffer_[begin].is_first_in_frame) {
			if(line_metadata_buffer_[begin].previous_frame_was_complete) {
				full_display_rectangle_.draw(0.0, 0.0, 0.0);
			}
			test_gl([&]{ glClear(GL_STENCIL_BUFFER_BIT); });
		}

		// Hunt for an end-of-frame.
		// TODO: eliminate this linear search by not loading frame data into LineMetadata.
		size_t end = begin;
		do {
			++end;
			if(end == line_metadata_buffer_.size()) end = 0;
		} while(end != area.end.line && !line_metadata_buffer_[end].is_first_in_frame);

		// Submit new lines.
		lines_.bind_all();
		const auto new_lines = ::submit(lines_, begin, end, line_buffer_);

		// Output new lines.
		line_output_shader_.bind();
		test_gl([&]{ glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, GLsizei(new_lines)); });

		begin = end;
	}
}

void ScanTarget::output_scans(const OutputArea &area) {
	if(area.end.scan == area.begin.scan) {
		return;
	}

	// Break scans into frames. This is tortured. TODO: resolve LineMetadata issues, as above.
	size_t scan_begin = area.begin.scan;
	size_t line_begin = area.begin.line;
	while(scan_begin != area.end.scan) {
		if(
			line_begin != area.end.line &&
			scan_begin == line_metadata_buffer_[line_begin].first_scan &&
			line_metadata_buffer_[line_begin].is_first_in_frame
		) {
			if(line_metadata_buffer_[line_begin].previous_frame_was_complete) {
				full_display_rectangle_.draw(0.0, 0.0, 0.0);
			}
			test_gl([&]{ glClear(GL_STENCIL_BUFFER_BIT); });
		}

		// Check for an end-of-frame in the current scan range by searching lines.
		// This is really messy.
		size_t line_end = line_begin;
		size_t scan_end = area.end.scan;
		while(true) {
			++line_end;
			if(line_end == line_metadata_buffer_.size()) line_end = 0;
			if(line_end == area.end.line) break;
			if(line_metadata_buffer_[line_end].is_first_in_frame) {
				scan_end = line_metadata_buffer_[line_end].first_scan;
				break;
			}
		}
		line_begin = line_end;

		// Submit and output new scans.
		scans_.bind_all();
		const auto new_scans = ::submit(scans_, scan_begin, scan_end, scan_buffer_);

		// Output new scans.
		scan_output_shader_.bind();
		test_gl([&]{ glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, GLsizei(new_scans)); });

		scan_begin = scan_end;
	}
}

void ScanTarget::draw(const int output_width, const int output_height) {
	while(is_drawing_to_output_.test_and_set(std::memory_order_acquire));

	if(!composition_buffer_.empty()) {
		// Copy the accumulation texture to the target.
		test_gl([&]{ glBindFramebuffer(GL_FRAMEBUFFER, target_framebuffer_); });
		test_gl([&]{ glViewport(0, 0, (GLsizei)output_width, (GLsizei)output_height); });
		copy_shader_.perform(OutputTextureUnit);
	}

	is_drawing_to_output_.clear(std::memory_order_release);
}
