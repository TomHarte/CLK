//
//  ScanTarget.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/11/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#include "ScanTarget.hpp"
#include "Primitives/Rectangle.hpp"

using namespace Outputs::Display::OpenGL;

namespace {

/// The texture unit from which to source 1bpp input data.
constexpr GLenum SourceData1BppTextureUnit = GL_TEXTURE0;
/// The texture unit from which to source 2bpp input data.
constexpr GLenum SourceData2BppTextureUnit = GL_TEXTURE1;
/// The texture unit from which to source 4bpp input data.
constexpr GLenum SourceData4BppTextureUnit = GL_TEXTURE2;

/// The texture unit which contains raw line-by-line composite, S-Video or RGB data.
constexpr GLenum UnprocessedLineBufferTextureUnit = GL_TEXTURE3;
/// The texture unit which contains line-by-line records of luminance and two channels of chrominance, straight after multiplication by the quadrature vector, not yet filtered.
constexpr GLenum SVideoLineBufferTextureUnit = GL_TEXTURE4;
/// The texture unit which contains line-by-line records of RGB.
constexpr GLenum RGBLineBufferTextureUnit = GL_TEXTURE5;

/// The texture unit that contains the current display.
constexpr GLenum AccumulationTextureUnit = GL_TEXTURE6;

#define TextureAddress(x, y)	(((y) << 11) | (x))
#define TextureAddressGetY(v)	uint16_t((v) >> 11)
#define TextureAddressGetX(v)	uint16_t((v) & 0x7ff)
#define TextureSub(a, b)		(((a) - (b)) & 0x3fffff)

const GLint internalFormatForDepth(std::size_t depth) {
	switch(depth) {
		default: return GL_FALSE;
		case 1: return GL_R8UI;
		case 2: return GL_RG8UI;
		case 3: return GL_RGB8UI;
		case 4: return GL_RGBA8UI;
	}
}

const GLenum formatForDepth(std::size_t depth) {
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
	glGenBuffers(1, &buffer_name);
	glBindBuffer(GL_ARRAY_BUFFER, buffer_name);
	glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(buffer_size), NULL, GL_STREAM_DRAW);

	glGenVertexArrays(1, &vertex_array_name);
	glBindVertexArray(vertex_array_name);
	glBindBuffer(GL_ARRAY_BUFFER, buffer_name);
}

ScanTarget::ScanTarget() :
	unprocessed_line_texture_(LineBufferWidth, LineBufferHeight, UnprocessedLineBufferTextureUnit, GL_NEAREST, false),
	full_display_rectangle_(-1.0f, -1.0f, 2.0f, 2.0f) {

	// Ensure proper initialisation of the two atomic pointer sets.
	read_pointers_.store(write_pointers_);
	submit_pointers_.store(write_pointers_);

	// Allocate space for the scans and lines.
	allocate_buffer(scan_buffer_, scan_buffer_name_, scan_vertex_array_);
	allocate_buffer(line_buffer_, line_buffer_name_, line_vertex_array_);

	// TODO: if this is OpenGL 4.4 or newer, use glBufferStorage rather than glBufferData
	// and specify GL_MAP_PERSISTENT_BIT. Then map the buffer now, and let the client
	// write straight into it.

	glGenTextures(1, &write_area_texture_name_);

	glBlendFunc(GL_SRC_ALPHA, GL_CONSTANT_COLOR);
	glBlendColor(0.4f, 0.4f, 0.4f, 1.0f);

	is_drawing_.clear();
}

ScanTarget::~ScanTarget() {
	while(is_drawing_.test_and_set()) {}
	glDeleteBuffers(1, &scan_buffer_name_);
	glDeleteTextures(1, &write_area_texture_name_);
	glDeleteVertexArrays(1, &scan_vertex_array_);
}

void ScanTarget::set_modals(Modals modals) {
	modals_ = modals;

	const auto data_type_size = Outputs::Display::size_for_data_type(modals.input_data_type);
	if(data_type_size != data_type_size_) {
		// TODO: flush output.

		data_type_size_ = data_type_size;
		write_area_texture_.resize(2048*2048*data_type_size_);

		write_pointers_.scan_buffer = 0;
		write_pointers_.write_area = 0;
	}

	// Pick a processing width; this will be at least four times the
	// colour subcarrier, and an integer multiple of the pixel clock and
	// at most 2048.
	const int colour_cycle_width = (modals.colour_cycle_numerator * 4 + modals.colour_cycle_denominator - 1) / modals.colour_cycle_denominator;
	const int dot_clock = modals.cycles_per_line / modals.clocks_per_pixel_greatest_common_divisor;
	const int overflow = colour_cycle_width % dot_clock;
	processing_width_ = colour_cycle_width + (overflow ? dot_clock - overflow : 0);
	processing_width_ = std::min(processing_width_, 2048);

	// Establish an output shader. TODO: add gamma correction here.
	output_shader_.reset(new Shader(
		glsl_globals(ShaderType::Line) + glsl_default_vertex_shader(ShaderType::Line),
		"#version 150\n"

		"out vec4 fragColour;"
		"in vec2 textureCoordinate;"

		"uniform sampler2D textureName;"

		"void main(void) {"
			"fragColour = vec4(texture(textureName, textureCoordinate).rgb, 0.64);"
		"}",
		attribute_bindings(ShaderType::Line)
	));

	glBindVertexArray(line_vertex_array_);
	glBindBuffer(GL_ARRAY_BUFFER, line_buffer_name_);
	enable_vertex_attributes(ShaderType::Line, *output_shader_);
	set_uniforms(ShaderType::Line, *output_shader_);
	output_shader_->set_uniform("origin", modals.visible_area.origin.x, modals.visible_area.origin.y);
	output_shader_->set_uniform("size", modals.visible_area.size.width, modals.visible_area.size.height);

	// Establish such intermediary shaders as are required.
	pipeline_stages_.clear();
	if(modals_.display_type == DisplayType::CompositeColour) {
		pipeline_stages_.emplace_back(
			composite_to_svideo_shader(modals_.colour_cycle_numerator, modals_.colour_cycle_denominator, processing_width_).release(),
			SVideoLineBufferTextureUnit,
			GL_NEAREST);
	}
	if(modals_.display_type == DisplayType::SVideo || modals_.display_type == DisplayType::CompositeColour) {
		pipeline_stages_.emplace_back(
			svideo_to_rgb_shader(modals_.colour_cycle_numerator, modals_.colour_cycle_denominator, processing_width_).release(),
			(modals_.display_type == DisplayType::CompositeColour) ? RGBLineBufferTextureUnit : SVideoLineBufferTextureUnit,
			GL_NEAREST);
	}

	glBindVertexArray(scan_vertex_array_);
	glBindBuffer(GL_ARRAY_BUFFER, scan_buffer_name_);

	// Establish an input shader.
	input_shader_ = input_shader(modals_.input_data_type, modals_.display_type);
	enable_vertex_attributes(ShaderType::InputScan, *input_shader_);
	set_uniforms(ShaderType::InputScan, *input_shader_);
	input_shader_->set_uniform("textureName", GLint(SourceData1BppTextureUnit - GL_TEXTURE0));

	// Cascade the texture units in use as per the pipeline stages.
	std::vector<Shader *> input_shaders = {input_shader_.get()};
	GLint texture_unit = GLint(UnprocessedLineBufferTextureUnit - GL_TEXTURE0);
	for(const auto &stage: pipeline_stages_) {
		input_shaders.push_back(stage.shader.get());

		stage.shader->set_uniform("textureName", texture_unit);
		set_uniforms(ShaderType::ProcessedScan, *stage.shader);
		enable_vertex_attributes(ShaderType::ProcessedScan, *stage.shader);

		++texture_unit;
	}
	output_shader_->set_uniform("textureName", texture_unit);

	// Ensure that all shaders involved in the input pipeline have the proper colour space knowledged.
	for(auto shader: input_shaders) {
		switch(modals.composite_colour_space) {
			case ColourSpace::YIQ: {
				const GLfloat rgbToYIQ[] = {0.299f, 0.596f, 0.211f, 0.587f, -0.274f, -0.523f, 0.114f, -0.322f, 0.312f};
				const GLfloat yiqToRGB[] = {1.0f, 1.0f, 1.0f, 0.956f, -0.272f, -1.106f, 0.621f, -0.647f, 1.703f};
				shader->set_uniform_matrix("lumaChromaToRGB", 3, false, yiqToRGB);
				shader->set_uniform_matrix("rgbToLumaChroma", 3, false, rgbToYIQ);
			} break;

			case ColourSpace::YUV: {
				const GLfloat rgbToYUV[] = {0.299f, -0.14713f, 0.615f, 0.587f, -0.28886f, -0.51499f, 0.114f, 0.436f, -0.10001f};
				const GLfloat yuvToRGB[] = {1.0f, 1.0f, 1.0f, 0.0f, -0.39465f, 2.03211f, 1.13983f, -0.58060f, 0.0f};
				shader->set_uniform_matrix("lumaChromaToRGB", 3, false, yuvToRGB);
				shader->set_uniform_matrix("rgbToLumaChroma", 3, false, rgbToYUV);
			} break;
		}
	}
}

void Outputs::Display::OpenGL::ScanTarget::set_uniforms(ShaderType type, Shader &target) {
	// Slightly over-amping rowHeight here is a cheap way to make sure that lines
	// converge even allowing for the fact that they may not be spaced by exactly
	// the expected distance. Cf. the stencil-powered logic for making sure all
	// pixels are painted only exactly once per field.
	target.set_uniform("rowHeight", GLfloat(1.05f / modals_.expected_vertical_lines));
	target.set_uniform("scale", GLfloat(modals_.output_scale.x), GLfloat(modals_.output_scale.y));
	target.set_uniform("processingWidth", GLfloat(processing_width_) / 2048.0f);
	target.set_uniform("phaseOffset", GLfloat(modals_.input_data_tweaks.phase_linked_luminance_offset));
}

Outputs::Display::ScanTarget::Scan *ScanTarget::begin_scan() {
	if(allocation_has_failed_) return nullptr;

	const auto result = &scan_buffer_[write_pointers_.scan_buffer];
	const auto read_pointers = read_pointers_.load();

	// Advance the pointer.
	const auto next_write_pointer = decltype(write_pointers_.scan_buffer)((write_pointers_.scan_buffer + 1) % scan_buffer_.size());

	// Check whether that's too many.
	if(next_write_pointer == read_pointers.scan_buffer) {
		allocation_has_failed_ = true;
		return nullptr;
	}
	write_pointers_.scan_buffer = next_write_pointer;
	++provided_scans_;

	// Fill in extra OpenGL-specific details.
	result->line = write_pointers_.line;

	vended_scan_ = result;
	return &result->scan;
}

void ScanTarget::end_scan() {
	if(vended_scan_) {
		vended_scan_->data_y = TextureAddressGetY(vended_write_area_pointer_);
		vended_scan_->line = write_pointers_.line;
		vended_scan_->scan.end_points[0].data_offset += TextureAddressGetX(vended_write_area_pointer_);
		vended_scan_->scan.end_points[1].data_offset += TextureAddressGetX(vended_write_area_pointer_);
    }
    vended_scan_ = nullptr;
}

uint8_t *ScanTarget::begin_data(size_t required_length, size_t required_alignment) {
	if(allocation_has_failed_) return nullptr;

	// Determine where the proposed write area would start and end.
	uint16_t output_y = TextureAddressGetY(write_pointers_.write_area);

	uint16_t aligned_start_x = TextureAddressGetX(write_pointers_.write_area & 0xffff) + 1;
	aligned_start_x += uint16_t((required_alignment - aligned_start_x%required_alignment)%required_alignment);

	uint16_t end_x = aligned_start_x + uint16_t(1 + required_length);

	if(end_x > WriteAreaWidth) {
		output_y = (output_y + 1) % WriteAreaHeight;
		aligned_start_x = uint16_t(required_alignment);
		end_x = aligned_start_x + uint16_t(1 + required_length);
	}

	// Check whether that steps over the read pointer.
	const auto end_address = TextureAddress(end_x, output_y);
	const auto read_pointers = read_pointers_.load();

	const auto end_distance = TextureSub(end_address, read_pointers.write_area);
	const auto previous_distance = TextureSub(write_pointers_.write_area, read_pointers.write_area);

	// If allocating this would somehow make the write pointer back away from the read pointer,
	// there must not be enough space left.
	if(end_distance < previous_distance) {
		allocation_has_failed_ = true;
		return nullptr;
	}

	// Everything checks out, return the pointer.
	vended_write_area_pointer_ = write_pointers_.write_area = TextureAddress(aligned_start_x, output_y);
	return &write_area_texture_[size_t(write_pointers_.write_area) * data_type_size_];

	// Note state at exit:
	//		write_pointers_.write_area points to the first pixel the client is expected to draw to.
}

void ScanTarget::end_data(size_t actual_length) {
	if(allocation_has_failed_) return;

	// Bookend the start of the new data, to safeguard for precision errors in sampling.
	memcpy(
		&write_area_texture_[size_t(write_pointers_.write_area - 1) * data_type_size_],
		&write_area_texture_[size_t(write_pointers_.write_area) * data_type_size_],
		data_type_size_);

	// The write area was allocated in the knowledge that there's sufficient
	// distance left on the current line, so there's no need to worry about carry.
	write_pointers_.write_area += actual_length + 1;

	// Also bookend the end.
	memcpy(
		&write_area_texture_[size_t(write_pointers_.write_area - 1) * data_type_size_],
		&write_area_texture_[size_t(write_pointers_.write_area - 2) * data_type_size_],
		data_type_size_);
}

void ScanTarget::submit() {
	if(allocation_has_failed_) {
		// Reset all pointers to where they were; this also means
		// the stencil won't be properly populated.
		write_pointers_ = submit_pointers_.load();
		frame_was_complete_ = false;
	} else {
		// Advance submit pointer.
		submit_pointers_.store(write_pointers_);
	}

	allocation_has_failed_ = false;
}

void ScanTarget::announce(Event event, uint16_t x, uint16_t y) {
	switch(event) {
		default: break;
		case ScanTarget::Event::BeginHorizontalRetrace:
			if(active_line_) {
				active_line_->end_points[1].x = x;
				active_line_->end_points[1].y = y;
			}
		break;
		case ScanTarget::Event::EndHorizontalRetrace: {
			// Commit the most recent line only if any scans fell on it.
			// Otherwise there's no point outputting it, it'll contribute nothing.
			if(provided_scans_) {
				// Store metadata if concluding a previous line.
				if(active_line_) {
					line_metadata_buffer_[size_t(write_pointers_.line)].is_first_in_frame = is_first_in_frame_;
					line_metadata_buffer_[size_t(write_pointers_.line)].previous_frame_was_complete = frame_was_complete_;
					is_first_in_frame_ = false;
				}

				const auto read_pointers = read_pointers_.load();

				// Attempt to allocate a new line; note allocation failure if necessary.
				const auto next_line = uint16_t((write_pointers_.line + 1) % LineBufferHeight);
				if(next_line == read_pointers.line) {
					allocation_has_failed_ = true;
					active_line_ = nullptr;
				} else {
					write_pointers_.line = next_line;
					active_line_ = &line_buffer_[size_t(write_pointers_.line)];
				}
				provided_scans_ = 0;
			}

			if(active_line_) {
				active_line_->end_points[0].x = x;
				active_line_->end_points[0].y = y;
				active_line_->line = write_pointers_.line;
			}
		} break;
		case ScanTarget::Event::EndVerticalRetrace:
			is_first_in_frame_ = true;
			frame_was_complete_ = true;
		break;
	}

	// TODO: any lines that include any portion of vertical sync should be hidden.
	// (maybe set a flag and zero out the line coordinates?)
}

void ScanTarget::draw(bool synchronous, int output_width, int output_height) {
	if(fence_ != nullptr) {
		// if the GPU is still busy, don't wait; we'll catch it next time
		if(glClientWaitSync(fence_, GL_SYNC_FLUSH_COMMANDS_BIT, synchronous ? GL_TIMEOUT_IGNORED : 0) == GL_TIMEOUT_EXPIRED) {
			return;
		}
		fence_ = nullptr;
	}

	// Spin until the is-drawing flag is reset; the wait sync above will deal
	// with instances where waiting is inappropriate.
	while(is_drawing_.test_and_set());

	// Grab the current read and submit pointers.
	const auto submit_pointers = submit_pointers_.load();
	const auto read_pointers = read_pointers_.load();

	// Submit scans; only the new ones need to be communicated.
	size_t new_scans = (submit_pointers.scan_buffer + scan_buffer_.size() - read_pointers.scan_buffer) % scan_buffer_.size();
	if(new_scans) {
		glBindBuffer(GL_ARRAY_BUFFER, scan_buffer_name_);

		// Map only the required portion of the buffer.
		const size_t new_scans_size = new_scans * sizeof(Scan);
		uint8_t *const destination = static_cast<uint8_t *>(
			glMapBufferRange(GL_ARRAY_BUFFER, 0, GLsizeiptr(new_scans_size), GL_MAP_WRITE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT)
		);

		if(read_pointers.scan_buffer < submit_pointers.scan_buffer) {
			memcpy(destination, &scan_buffer_[read_pointers.scan_buffer], new_scans_size);
		} else {
			const size_t first_portion_length = (scan_buffer_.size() - read_pointers.scan_buffer) * sizeof(Scan);
			memcpy(destination, &scan_buffer_[read_pointers.scan_buffer], first_portion_length);
			memcpy(&destination[first_portion_length], &scan_buffer_[0], new_scans_size - first_portion_length);
		}

		// Flush and unmap the buffer.
		glFlushMappedBufferRange(GL_ARRAY_BUFFER, 0, GLsizeiptr(new_scans_size));
		glUnmapBuffer(GL_ARRAY_BUFFER);
	}

	// Submit texture.
	if(submit_pointers.write_area != read_pointers.write_area) {
		glActiveTexture(SourceData1BppTextureUnit);
		glBindTexture(GL_TEXTURE_2D, write_area_texture_name_);

		// Create storage for the texture if it doesn't yet exist; this was deferred until here
		// because the pixel format wasn't initially known.
		if(!texture_exists_) {
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexImage2D(
				GL_TEXTURE_2D,
				0,
				internalFormatForDepth(data_type_size_),
				WriteAreaWidth,
				WriteAreaHeight,
				0,
				formatForDepth(data_type_size_),
				GL_UNSIGNED_BYTE,
				nullptr);
			texture_exists_ = true;
		}

		const auto start_y = TextureAddressGetY(read_pointers.write_area);
		const auto end_y = TextureAddressGetY(submit_pointers.write_area);
		if(end_y >= start_y) {
			// Submit the direct region from the submit pointer to the read pointer.
			glTexSubImage2D(	GL_TEXTURE_2D, 0,
								0, start_y,
								WriteAreaWidth,
								1 + end_y - start_y,
								formatForDepth(data_type_size_),
								GL_UNSIGNED_BYTE,
								&write_area_texture_[size_t(TextureAddress(0, start_y)) * data_type_size_]);
		} else {
			// The circular buffer wrapped around; submit the data from the read pointer to the end of
			// the buffer and from the start of the buffer to the submit pointer.
			glTexSubImage2D(	GL_TEXTURE_2D, 0,
								0, 0,
								WriteAreaWidth,
								1 + end_y,
								formatForDepth(data_type_size_),
								GL_UNSIGNED_BYTE,
								&write_area_texture_[0]);
			glTexSubImage2D(	GL_TEXTURE_2D, 0,
								0, start_y,
								WriteAreaWidth,
								WriteAreaHeight - start_y,
								formatForDepth(data_type_size_),
								GL_UNSIGNED_BYTE,
								&write_area_texture_[size_t(TextureAddress(0, start_y)) * data_type_size_]);
		}
	}

	// Push new input to the unprocessed line buffer.
	if(new_scans) {
		glDisable(GL_BLEND);
		unprocessed_line_texture_.bind_framebuffer();

		// Clear newly-touched lines; that is everything from (read+1) to submit.
		const uint16_t first_line_to_clear = (read_pointers.line+1)%line_buffer_.size();
		const uint16_t final_line_to_clear = submit_pointers.line;
		if(first_line_to_clear != final_line_to_clear) {
			glEnable(GL_SCISSOR_TEST);

			if(first_line_to_clear < final_line_to_clear) {
				glScissor(0, first_line_to_clear, unprocessed_line_texture_.get_width(), final_line_to_clear - first_line_to_clear);
				glClear(GL_COLOR_BUFFER_BIT);

				if(pipeline_stages_.size()) {
					pipeline_stages_.back().target.bind_framebuffer();
					glClear(GL_COLOR_BUFFER_BIT);
					unprocessed_line_texture_.bind_framebuffer();
				}
			} else {
				glScissor(0, 0, unprocessed_line_texture_.get_width(), final_line_to_clear);
				glClear(GL_COLOR_BUFFER_BIT);
				glScissor(0, first_line_to_clear, unprocessed_line_texture_.get_width(), unprocessed_line_texture_.get_height() - first_line_to_clear);
				glClear(GL_COLOR_BUFFER_BIT);

				if(pipeline_stages_.size()) {
					pipeline_stages_.back().target.bind_framebuffer();
					glScissor(0, 0, unprocessed_line_texture_.get_width(), final_line_to_clear);
					glClear(GL_COLOR_BUFFER_BIT);
					glScissor(0, first_line_to_clear, unprocessed_line_texture_.get_width(), unprocessed_line_texture_.get_height() - first_line_to_clear);
					glClear(GL_COLOR_BUFFER_BIT);
					unprocessed_line_texture_.bind_framebuffer();
				}
			}

			glDisable(GL_SCISSOR_TEST);
		}

		// Apply new spans. They definitely always go to the first buffer.
		glBindVertexArray(scan_vertex_array_);
		input_shader_->bind();
		glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, GLsizei(new_scans));

		// If there are any further pipeline stages, apply them.
		for(auto &stage: pipeline_stages_) {
			stage.target.bind_framebuffer();
			stage.shader->bind();
			glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, GLsizei(new_scans));
		}
	}

	// Ensure the accumulation buffer is properly sized.
	const int proportional_width = (output_height * 4) / 3;
	if(!accumulation_texture_ || (	/* !synchronous && */ (accumulation_texture_->get_width() != proportional_width || accumulation_texture_->get_height() != output_height))) {
		std::unique_ptr<OpenGL::TextureTarget> new_framebuffer(
			new TextureTarget(
				GLsizei(proportional_width),
				GLsizei(output_height),
				AccumulationTextureUnit,
				GL_LINEAR,
				true));
		if(accumulation_texture_) {
			new_framebuffer->bind_framebuffer();
			glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

			glActiveTexture(AccumulationTextureUnit);
			accumulation_texture_->bind_texture();
			accumulation_texture_->draw(float(output_width) / float(output_height));

			glClear(GL_STENCIL_BUFFER_BIT);

			new_framebuffer->bind_texture();
		}
		accumulation_texture_ = std::move(new_framebuffer);

		// In the absence of a way to resize a stencil buffer, just mark
		// what's currently present as invalid to avoid an improper clear
		// for this frame.
		stencil_is_valid_ = false;
	}

	// Figure out how many new spans are ostensible ready; use two less than that.
	uint16_t new_spans = (submit_pointers.line + LineBufferHeight - read_pointers.line) % LineBufferHeight;
	if(new_spans) {
		// Bind the accumulation framebuffer.
		accumulation_texture_->bind_framebuffer();

		// Enable blending and stenciling, and ensure spans increment the stencil buffer.
		glEnable(GL_BLEND);
		glEnable(GL_STENCIL_TEST);
		glStencilFunc(GL_EQUAL, 0, GLuint(-1));
		glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);

		// Prepare to output lines.
		glBindVertexArray(line_vertex_array_);
		output_shader_->bind();

		// Prepare to upload data that will consitute lines.
		glBindBuffer(GL_ARRAY_BUFFER, line_buffer_name_);

		// Divide spans by which frame they're in.
		uint16_t start_line = read_pointers.line;
		while(new_spans) {
			uint16_t end_line = start_line+1;

			// Find the limit of spans to draw in this cycle.
			size_t spans = 1;
			while(end_line != submit_pointers.line && !line_metadata_buffer_[end_line].is_first_in_frame) {
				end_line = (end_line + 1) % LineBufferHeight;
				++spans;
			}

			// If this is start-of-frame, clear any untouched pixels and flush the stencil buffer
			if(line_metadata_buffer_[start_line].is_first_in_frame) {
				if(stencil_is_valid_ && line_metadata_buffer_[start_line].previous_frame_was_complete) {
					full_display_rectangle_.draw(0.0f, 0.0f, 0.0f);
				}
				stencil_is_valid_ = true;
				glClear(GL_STENCIL_BUFFER_BIT);

				// Rebind the program for span output.
				glBindVertexArray(line_vertex_array_);
				output_shader_->bind();
			}

			// Upload and draw.
			const auto buffer_size = spans * sizeof(Line);
			if(!end_line || end_line > start_line) {
				glBufferSubData(GL_ARRAY_BUFFER, 0, GLsizeiptr(buffer_size), &line_buffer_[start_line]);
			} else {
				uint8_t *destination = static_cast<uint8_t *>(
					glMapBufferRange(GL_ARRAY_BUFFER, 0, GLsizeiptr(buffer_size), GL_MAP_WRITE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT)
				);
				assert(destination);

				const size_t buffer_length = line_buffer_.size() * sizeof(Line);
				const size_t start_position = start_line * sizeof(Line);
				memcpy(&destination[0], &line_buffer_[start_line], buffer_length - start_position);
				memcpy(&destination[buffer_length - start_position], &line_buffer_[0], end_line * sizeof(Line));

				glFlushMappedBufferRange(GL_ARRAY_BUFFER, 0, GLsizeiptr(buffer_size));
				glUnmapBuffer(GL_ARRAY_BUFFER);
			}

			glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, GLsizei(spans));

			start_line = end_line;
			new_spans -= spans;
		}

		// Disable blending and the stencil test again.
		glDisable(GL_STENCIL_TEST);
		glDisable(GL_BLEND);
	}

	// Copy the accumulatiion texture to the target (TODO: don't assume framebuffer 0).
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, (GLsizei)output_width, (GLsizei)output_height);

	glClear(GL_COLOR_BUFFER_BIT);
	accumulation_texture_->bind_texture();
	accumulation_texture_->draw(float(output_width) / float(output_height), 4.0f / 255.0f);

	// All data now having been spooled to the GPU, update the read pointers to
	// the submit pointer location.
	read_pointers_.store(submit_pointers);

	// Grab a fence sync object to avoid busy waiting upon the next extry into this
	// function, and reset the is_drawing_ flag.
	fence_ = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
	is_drawing_.clear();
}
