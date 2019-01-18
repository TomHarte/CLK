//
//  ScanTarget.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/11/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#ifndef ScanTarget_hpp
#define ScanTarget_hpp

#include "../ScanTarget.hpp"
#include "OpenGL.hpp"
#include "Primitives/TextureTarget.hpp"
#include "Primitives/Rectangle.hpp"

#include "../../SignalProcessing/FIRFilter.hpp"

#include <array>
#include <atomic>
#include <cstdint>
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
class ScanTarget: public Outputs::Display::ScanTarget {
	public:
		ScanTarget(GLuint target_framebuffer = 0, float output_gamma = 2.2f);
		~ScanTarget();

		void set_target_framebuffer(GLuint);

		void draw(bool synchronous, int output_width, int output_height);

	private:
		static constexpr int WriteAreaWidth = 2048;
		static constexpr int WriteAreaHeight = 2048;

		static constexpr int LineBufferWidth = 2048;
		static constexpr int LineBufferHeight = 2048;

		GLuint target_framebuffer_;
		const float output_gamma_;

		// Outputs::Display::ScanTarget overrides.
		void set_modals(Modals) override;
		Scan *begin_scan() override;
		void end_scan() override;
		uint8_t *begin_data(size_t required_length, size_t required_alignment) override;
		void end_data(size_t actual_length) override;
		void submit() override;
		void announce(Event event, bool is_visible, const Outputs::Display::ScanTarget::Scan::EndPoint &location, uint8_t colour_burst_amplitude) override;

		bool output_is_visible_ = false;

		// Extends the definition of a Scan to include two extra fields,
		// relevant to the way that this scan target processes video.
		struct Scan {
			Outputs::Display::ScanTarget::Scan scan;

			/// Stores the y coordinate that this scan's data is at, within the write area texture.
			uint16_t data_y;
			/// Stores the y coordinate of this scan within the line buffer.
			uint16_t line;
		};

		struct PointerSet {
			// The sizes below might be less hassle as something more natural like ints,
			// but squeezing this struct into 64 bits makes the std::atomics more likely
			// to be lock free; they are under LLVM x86-64.
			int write_area = 0;
			uint16_t scan_buffer = 0;
			uint16_t line = 0;
		};

		/// A pointer to the next thing that should be provided to the caller for data.
		PointerSet write_pointers_;

		/// A pointer to the final thing currently cleared for submission.
		std::atomic<PointerSet> submit_pointers_;

		/// A pointer to the first thing not yet submitted for display.
		std::atomic<PointerSet> read_pointers_;

		// Maintains a buffer of the most recent 3072 scans.
		std::array<Scan, 3072> scan_buffer_;

		// Maintains a list of composite scan buffer coordinates; the Line struct
		// is transported to the GPU in its entirety; the LineMetadatas live in CPU
		// space only.
		struct Line {
			struct EndPoint {
				uint16_t x, y;
				uint16_t cycles_since_end_of_horizontal_retrace;
				int16_t composite_angle;
			} end_points[2];
			uint16_t line;
			uint8_t composite_amplitude;
		};
		struct LineMetadata {
			bool is_first_in_frame;
			bool previous_frame_was_complete;
		};
		std::array<Line, LineBufferHeight> line_buffer_;
		std::array<LineMetadata, LineBufferHeight> line_metadata_buffer_;

		// Contains the first composition of scans into lines;
		// they're accumulated prior to output to allow for continuous
		// application of any necessary conversions — e.g. composite processing.
		TextureTarget unprocessed_line_texture_;

		// Scans are accumulated to the accumulation texture; the full-display
		// rectangle is used to ensure untouched pixels properly decay.
		std::unique_ptr<TextureTarget> accumulation_texture_;
		Rectangle full_display_rectangle_;
		bool stencil_is_valid_ = false;

		// Ephemeral state that helps in line composition.
		Line *active_line_ = nullptr;
		int provided_scans_ = 0;
		bool is_first_in_frame_ = true;
		bool frame_is_complete_ = true;
		bool previous_frame_was_complete_ = true;

		// OpenGL storage handles for buffer data.
		GLuint scan_buffer_name_ = 0, scan_vertex_array_ = 0;
		GLuint line_buffer_name_ = 0, line_vertex_array_ = 0;

		template <typename T> void allocate_buffer(const T &array, GLuint &buffer_name, GLuint &vertex_array_name);
		template <typename T> void patch_buffer(const T &array, GLuint target, uint16_t submit_pointer, uint16_t read_pointer);

		// Uses a texture to vend write areas.
		std::vector<uint8_t> write_area_texture_;
		size_t data_type_size_ = 0;

		GLuint write_area_texture_name_ = 0;
		bool texture_exists_ = false;

		// Ephemeral information for the begin/end functions.
		Scan *vended_scan_ = nullptr;
		int vended_write_area_pointer_ = 0;

		// Track allocation failures.
		bool allocation_has_failed_ = false;

		// Receives scan target modals.
		Modals modals_;
		bool modals_are_dirty_ = false;
		void setup_pipeline();

		enum class ShaderType {
			Composition,
			Conversion
		};

		/*!
			Calls @c taret.enable_vertex_attribute_with_pointer to attach all
			globals for shaders of @c type to @c target.
		*/
		static void enable_vertex_attributes(ShaderType type, Shader &target);
		void set_uniforms(ShaderType type, Shader &target);

		GLsync fence_ = nullptr;
		std::atomic_flag is_drawing_;

		int processing_width_ = 0;
		std::unique_ptr<Shader> input_shader_;
		std::unique_ptr<Shader> output_shader_;

		/*!
			Produces a shader that composes fragment of the input stream to a single buffer,
			normalising the data into one of four forms: RGB, 8-bit luminance,
			phase-linked luminance or luminance+phase offset.
		*/
		static std::unique_ptr<Shader> composition_shader(InputDataType input_data_type);
		/*!
			Produces a shader that reads from a composition buffer and converts to host
			output RGB, decoding composite or S-Video as necessary.
		*/
		static std::unique_ptr<Shader> conversion_shader(InputDataType input_data_type, DisplayType display_type, ColourSpace colour_space, float gamma_ratio, float brightness);
};

}
}
}

#endif /* ScanTarget_hpp */
