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

constexpr int WriteAreaWidth = 2048;
constexpr int WriteAreaHeight = 2048;

constexpr int LineBufferWidth = 2048;
constexpr int LineBufferHeight = 2048;

/// The texture unit from which to source 1bpp input data.
constexpr GLenum SourceData1BppTextureUnit = GL_TEXTURE0;
/// The texture unit from which to source 2bpp input data.
constexpr GLenum SourceData2BppTextureUnit = GL_TEXTURE1;
/// The texture unit from which to source 4bpp input data.
constexpr GLenum SourceData4BppTextureUnit = GL_TEXTURE2;

/// The texture unit which contains raw line-by-line composite or RGB data.
constexpr GLenum UnprocessedLineBufferTextureUnit = GL_TEXTURE3;
/// The texture unit which contains line-by-line records of luminance and amplitude-modulated chrominance.
constexpr GLenum CompositeSeparatedTextureUnit = GL_TEXTURE4;
/// The texture unit which contains line-by-line records of luminance and demodulated chrominance.
constexpr GLenum DemodulatedCompositeTextureUnit = GL_TEXTURE5;

/// The texture unit which contains line-by-line RGB.
constexpr GLenum LineBufferTextureUnit = GL_TEXTURE6;

/// The texture unit that contains the current display.
constexpr GLenum AccumulationTextureUnit = GL_TEXTURE7;

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

ScanTarget::ScanTarget() :
 	unprocessed_line_texture_(LineBufferWidth, LineBufferHeight, UnprocessedLineBufferTextureUnit, GL_LINEAR) {

	// Allocate space for the scans.
	const auto buffer_size = scan_buffer_.size() * sizeof(Scan);
	glGenBuffers(1, &scan_buffer_name_);
	glBindBuffer(GL_ARRAY_BUFFER, scan_buffer_name_);
	glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(buffer_size), NULL, GL_STREAM_DRAW);

	// TODO: if this is OpenGL 4.4 or newer, use glBufferStorage rather than glBufferData
	// and specify GL_MAP_PERSISTENT_BIT. Then map the buffer now, and let the client
	// write straight into it.

	glGenTextures(1, &write_area_texture_name_);
	glGenVertexArrays(1, &scan_vertex_array_);

	glBindVertexArray(scan_vertex_array_);
	glBindBuffer(GL_ARRAY_BUFFER, scan_buffer_name_);
	test_shader_.reset(new Shader(
		globals(ShaderType::Scan),
		"#version 150\n"
		"out vec4 fragColour;"
		"void main(void) {"
			"fragColour = vec4(1.0);"
		"}"
	));
	enable_vertex_attributes(ShaderType::Scan, *test_shader_);
}

ScanTarget::~ScanTarget() {
	glDeleteBuffers(1, &scan_buffer_name_);
	glDeleteTextures(1, &write_area_texture_name_);
	glDeleteVertexArrays(1, &scan_vertex_array_);
}

void ScanTarget::set_modals(Modals modals) {
	// TODO: consider resizing the write_area_texture_, and setting
	// write_area_texture_line_length_ appropriately.
	modals_ = modals;

	const auto data_type_size = Outputs::Display::size_for_data_type(modals.input_data_type);
	if(data_type_size != data_type_size_) {
		// TODO: flush output.

		data_type_size_ = data_type_size;
		write_area_texture_.resize(2048*2048*data_type_size_);

		write_pointers_.scan_buffer = 0;
		write_pointers_.write_area = 0;
	}

	// TODO: this, but not to the test shader.
	test_shader_->set_uniform("scale", GLfloat(modals.output_scale.x), GLfloat(modals.output_scale.y));
	test_shader_->set_uniform("rowHeight", GLfloat(1.0f / modals.expected_vertical_lines));
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

	// Fill in extra OpenGL-specific details.
	result->line = write_pointers_.line;

	vended_scan_ = result;
	return &result->scan;
}

void ScanTarget::end_scan() {
	if(vended_scan_) {
		vended_scan_->data_y = TextureAddressGetY(vended_write_area_pointer_);
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

	// The write area was allocated in the knowledge that there's sufficient
	// distance left on the current line, so there's no need to worry about carry.
	write_pointers_.write_area += actual_length + 1;
}

void ScanTarget::submit() {
	if(allocation_has_failed_) {
		// Reset all pointers to where they were.
		write_pointers_ = submit_pointers_.load();
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
				active_line_ = nullptr;
			}
		break;
		case ScanTarget::Event::EndHorizontalRetrace: {
			const auto read_pointers = read_pointers_.load();

			// Attempt to allocate a new line; note allocation failure if necessary.
			const auto next_line = uint16_t((write_pointers_.line + 1) % LineBufferHeight);

			// Check whether that's too many.
			if(next_line == read_pointers.line) {
				allocation_has_failed_ = true;
			} else {
				write_pointers_.line = next_line;
				active_line_ = &line_buffer_[size_t(write_pointers_.line)];
				active_line_->end_points[0].x = x;
				active_line_->end_points[0].y = y;
				active_line_->line = write_pointers_.line;
			}
		} break;
	}
}

void ScanTarget::draw() {
	glClear(GL_COLOR_BUFFER_BIT);

	// Grab the current read and submit pointers.
	const auto submit_pointers = submit_pointers_.load();
	const auto read_pointers = read_pointers_.load();

	// Submit scans.
	if(submit_pointers.scan_buffer != read_pointers.scan_buffer) {
		const auto buffer_size = scan_buffer_.size() * sizeof(Scan);
		uint8_t *destination = static_cast<uint8_t *>(
			glMapBufferRange(GL_ARRAY_BUFFER, 0, GLsizeiptr(buffer_size), GL_MAP_WRITE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT)
		);
		assert(destination);

		if(submit_pointers.scan_buffer > read_pointers.scan_buffer) {
			// Submit the direct region from the submit pointer to the read pointer.
			const size_t offset = read_pointers.scan_buffer * sizeof(Scan);
			const size_t length = (submit_pointers.scan_buffer - read_pointers.scan_buffer) * sizeof(Scan);
			memcpy(&destination[offset], &scan_buffer_[read_pointers.scan_buffer], length);

			glFlushMappedBufferRange(GL_ARRAY_BUFFER, GLintptr(offset), GLsizeiptr(length));
		} else {
			// The circular buffer wrapped around; submit the data from the read pointer to the end of
			// the buffer and from the start of the buffer to the submit pointer.
			const size_t offset = read_pointers.scan_buffer * sizeof(Scan);
			const size_t end_length = (scan_buffer_.size() - read_pointers.scan_buffer)  * sizeof(Scan);
			const size_t start_length = submit_pointers.scan_buffer * sizeof(Scan);

			memcpy(&destination[offset], &scan_buffer_[read_pointers.scan_buffer], end_length);
			memcpy(&destination[0], &scan_buffer_[0], start_length);

			glFlushMappedBufferRange(GL_ARRAY_BUFFER, GLintptr(offset), GLsizeiptr(end_length));
			glFlushMappedBufferRange(GL_ARRAY_BUFFER, 0, GLsizeiptr(start_length));
		}

		glUnmapBuffer(GL_ARRAY_BUFFER);
	}

	// Submit texture.
	if(submit_pointers.write_area != read_pointers.write_area) {
		glBindTexture(GL_TEXTURE_2D, write_area_texture_name_);

		// Create storage for the texture if it doesn't yet exist; this was deferred until here
		// because the pixel format wasn't initially known.
		if(!texture_exists_) {
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
								&write_area_texture_[size_t(TextureAddress(0, start_y))]);
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
								&write_area_texture_[size_t(TextureAddress(0, start_y))]);
		}
	}

	// TODO: clear composite buffer (if needed).
	// TODO: drawing (!)


	// All data now having been spooled to the GPU, update the read pointers to
	// the submit pointer location.
	read_pointers_.store(submit_pointers);

	// TEST: draw all scans.
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindVertexArray(scan_vertex_array_);
	test_shader_->bind();
	glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, GLsizei(scan_buffer_.size()));
//	Rectangle rect(-0.8f, -0.8f, 1.6f, 1.6f);
//	rect.draw(1, 1, 0);
}
