//
//  ScanTarget.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/11/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#pragma once

#include "Outputs/Log.hpp"
#include "Outputs/DisplayMetrics.hpp"
#include "Outputs/ScanTargets/BufferingScanTarget.hpp"

#include "API.hpp"
#include "OpenGL.hpp"

#include "Primitives/Texture.hpp"
#include "Primitives/TextureTarget.hpp"
#include "Primitives/VertexArray.hpp"

#include "Shaders/CopyShader.hpp"
#include "Shaders/CompositionShader.hpp"
#include "Shaders/DirtyZone.hpp"
#include "Shaders/LineOutputShader.hpp"
#include "Shaders/KernelShaders.hpp"
#include "Shaders/Rectangle.hpp"

#include "SignalProcessing/FIRFilter.hpp"

#include <array>
#include <atomic>
#include <cstdint>
#include <chrono>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Outputs::Display::OpenGL {

/*!
	Provides a ScanTarget that uses OpenGL to render its output;
	this uses various internal buffers so that the only geometry
	drawn to the target framebuffer is a quad.
*/
class ScanTarget: public Outputs::Display::BufferingScanTarget {	// TODO: use private inheritance and expose only display_metrics() and a custom cast?
public:
	ScanTarget(API, GLuint target_framebuffer = 0, float output_gamma = 2.2f);

	void set_target_framebuffer(GLuint);

	/*! Pushes the current state of output to the target framebuffer. */
	void draw(int output_width, int output_height);
	/*! Processes all the latest input, at a resolution suitable for later output to a framebuffer of the specified size. */
	void update(int output_width, int output_height);

private:
	API api_;
	static constexpr int LineBufferHeight = 2048;
	float output_gamma_;
	size_t lines_submitted_ = 0;

#ifndef NDEBUG
	struct OpenGLVersionDumper {
		OpenGLVersionDumper() {
			// Note the OpenGL version, as the first thing this class does prior to construction.
			Log::Logger<Log::Source::OpenGL>::info().append(
				"Constructing scan target with OpenGL %s; shading language version %s",
				glGetString(GL_VERSION),
				glGetString(GL_SHADING_LANGUAGE_VERSION));
		}
	} dumper_;
#endif

	GLuint target_framebuffer_;
	std::chrono::high_resolution_clock::time_point line_submission_begin_time_;

	// Scans are accumulated to the accumulation texture; the full-display
	// rectangle is used to ensure untouched pixels properly decay.
	Rectangle full_display_rectangle_;
	bool stencil_is_valid_ = false;

	// Receives scan target modals.
	std::optional<ScanTarget::Modals> existing_modals_;
	void setup_pipeline();
	void set_alphas();

	GLsync fence_ = nullptr;
	std::atomic_flag is_drawing_to_output_;

	/*!
		@returns true if the current display type is a 'soft' one, i.e. one in which
		contrast tends to be low, such as a composite colour display.
	*/
	bool is_soft_display_type();

	// Storage for the various buffers.
	std::vector<uint8_t> write_area_texture_;
	std::array<Scan, LineBufferHeight*5> scan_buffer_{};
	std::array<Line, LineBufferHeight> line_buffer_{};
	std::array<DirtyZone, 2> dirty_zones_buffer_{};

	VertexArray scans_;
	VertexArray lines_;
	VertexArray dirty_zones_;

	Texture source_texture_;
	TextureTarget composition_buffer_;
	TextureTarget separation_buffer_;
	TextureTarget demodulation_buffer_;

	std::array<TextureTarget, 2> output_buffers_;
	int output_buffer_ = 0;

	Shader composition_shader_;
	Shader separation_shader_;
	Shader demodulation_shader_;
	LineOutputShader line_output_shader_;
	ScanOutputShader scan_output_shader_;
	CopyShader copy_shader_;
	FillShader fill_shader_;

	void update_aspect_ratio_transformation();
	void process_to_rgb(const OutputArea &);
	void output_lines(const OutputArea &);
	void output_scans(const OutputArea &);
};

}
