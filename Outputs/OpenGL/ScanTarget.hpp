//
//  ScanTarget.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/11/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#ifndef ScanTarget_hpp
#define ScanTarget_hpp

#include "../Log.hpp"
#include "../DisplayMetrics.hpp"
#include "../ScanTargets/BufferingScanTarget.hpp"

#include "OpenGL.hpp"
#include "Primitives/TextureTarget.hpp"
#include "Primitives/Rectangle.hpp"

#include "../../SignalProcessing/FIRFilter.hpp"

#include <array>
#include <atomic>
#include <cstdint>
#include <chrono>
#include <list>
#include <memory>
#include <string>
#include <vector>

namespace Outputs {
namespace Display {
namespace OpenGL {


/*!
	Provides a ScanTarget that uses OpenGL to render its output;
	this uses various internal buffers so that the only geometry
	drawn to the target framebuffer is a quad.
*/
class ScanTarget: public Outputs::Display::BufferingScanTarget {	// TODO: use private inheritance and expose only display_metrics() and a custom cast?
	public:
		ScanTarget(GLuint target_framebuffer = 0, float output_gamma = 2.2f);
		~ScanTarget();

		void set_target_framebuffer(GLuint);

		/*! Pushes the current state of output to the target framebuffer. */
		void draw(int output_width, int output_height);
		/*! Processes all the latest input, at a resolution suitable for later output to a framebuffer of the specified size. */
		void update(int output_width, int output_height);

	private:
		static constexpr int LineBufferWidth = 2048;
		static constexpr int LineBufferHeight = 2048;

#ifndef NDEBUG
		struct OpenGLVersionDumper {
			OpenGLVersionDumper() {
				// Note the OpenGL version, as the first thing this class does prior to construction.
				LOG("Constructing scan target with OpenGL " << glGetString(GL_VERSION) << "; shading language version " << glGetString(GL_SHADING_LANGUAGE_VERSION));
			}
		} dumper_;
#endif

		GLuint target_framebuffer_;
		const float output_gamma_;

		int resolution_reduction_level_ = 1;
		int output_height_ = 0;

		size_t lines_submitted_ = 0;
		std::chrono::high_resolution_clock::time_point line_submission_begin_time_;

		// Contains the first composition of scans into lines;
		// they're accumulated prior to output to allow for continuous
		// application of any necessary conversions — e.g. composite processing.
		TextureTarget unprocessed_line_texture_;

		// Contains pre-lowpass-filtered chrominance information that is
		// part-QAM-demoduled, if dealing with a QAM data source.
		std::unique_ptr<TextureTarget> qam_chroma_texture_;

		// Scans are accumulated to the accumulation texture; the full-display
		// rectangle is used to ensure untouched pixels properly decay.
		std::unique_ptr<TextureTarget> accumulation_texture_;
		Rectangle full_display_rectangle_;
		bool stencil_is_valid_ = false;

		// OpenGL storage handles for buffer data.
		GLuint scan_buffer_name_ = 0, scan_vertex_array_ = 0;
		GLuint line_buffer_name_ = 0, line_vertex_array_ = 0;

		template <typename T> void allocate_buffer(const T &array, GLuint &buffer_name, GLuint &vertex_array_name);
		template <typename T> void patch_buffer(const T &array, GLuint target, uint16_t submit_pointer, uint16_t read_pointer);

		GLuint write_area_texture_name_ = 0;
		bool texture_exists_ = false;

		// Receives scan target modals.
		void setup_pipeline();

		enum class ShaderType {
			Composition,
			Conversion,
			QAMSeparation
		};

		/*!
			Calls @c taret.enable_vertex_attribute_with_pointer to attach all
			globals for shaders of @c type to @c target.
		*/
		static void enable_vertex_attributes(ShaderType type, Shader &target);
		void set_uniforms(ShaderType type, Shader &target) const;
		std::vector<std::string> bindings(ShaderType type) const;

		GLsync fence_ = nullptr;
		std::atomic_flag is_drawing_to_accumulation_buffer_;

		std::unique_ptr<Shader> input_shader_;
		std::unique_ptr<Shader> output_shader_;
		std::unique_ptr<Shader> qam_separation_shader_;

		/*!
			Produces a shader that composes fragment of the input stream to a single buffer,
			normalising the data into one of four forms: RGB, 8-bit luminance,
			phase-linked luminance or luminance+phase offset.
		*/
		std::unique_ptr<Shader> composition_shader() const;
		/*!
			Produces a shader that reads from a composition buffer and converts to host
			output RGB, decoding composite or S-Video as necessary.
		*/
		std::unique_ptr<Shader> conversion_shader() const;
		/*!
			Produces a shader that writes separated but not-yet filtered QAM components
			from the unprocessed line texture to the QAM chroma texture, at a fixed
			size of four samples per colour clock, point sampled.
		*/
		std::unique_ptr<Shader> qam_separation_shader() const;

		void set_sampling_window(int output_Width, int output_height, Shader &target);

		std::string sampling_function() const;

		/*!
			@returns true if the current display type is a 'soft' one, i.e. one in which
			contrast tends to be low, such as a composite colour display.
		*/
		bool is_soft_display_type();

		// Storage for the various buffers.
		std::vector<uint8_t> write_area_texture_;
		std::array<Scan, LineBufferHeight*5> scan_buffer_;
		std::array<Line, LineBufferHeight> line_buffer_;
		std::array<LineMetadata, LineBufferHeight> line_metadata_buffer_;
};

}
}
}

#endif /* ScanTarget_hpp */
