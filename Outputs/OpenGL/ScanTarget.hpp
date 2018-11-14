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

#include <array>
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace Outputs {
namespace Display {
namespace OpenGL {

class ScanTarget: public Outputs::Display::ScanTarget {
	public:
		ScanTarget();
		~ScanTarget();
		void draw(bool synchronous, int output_width, int output_height);

	private:
		static constexpr int WriteAreaWidth = 2048;
		static constexpr int WriteAreaHeight = 2048;

		static constexpr int LineBufferWidth = 2048;
		static constexpr int LineBufferHeight = 2048;

		// Outputs::Display::ScanTarget overrides.
		void set_modals(Modals) override;
		Scan *begin_scan() override;
		void end_scan() override;
		uint8_t *begin_data(size_t required_length, size_t required_alignment) override;
		void end_data(size_t actual_length) override;
		void submit() override;
		void announce(Event event, uint16_t x, uint16_t y) override;

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

		// Maintains a list of composite scan buffer coordinates.
		struct Line {
			struct EndPoint {
				uint16_t x, y;
			} end_points[2];
			uint16_t line;
		};
		std::array<Line, LineBufferHeight> line_buffer_;
		TextureTarget unprocessed_line_texture_;
		Line *active_line_ = nullptr;

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

		enum class ShaderType {
			Scan,
			Line
		};

		/*!
			@returns A string containing GLSL code describing the standard set of
				@c in and @c uniform variables to bind to the relevant struct
				from [...]OpenGL::ScanTarget and a vertex function to provide
				the standard varyings.
		*/
		std::string glsl_globals(ShaderType type);

		/*!
		*/
		std::string glsl_default_vertex_shader(ShaderType type);

		/*!
			Calls @c taret.enable_vertex_attribute_with_pointer to attach all
			globals for shaders of @c type to @c target.
		*/
		void enable_vertex_attributes(ShaderType type, Shader &target);
		void set_uniforms(ShaderType type, Shader &target);

		GLsync fence_ = nullptr;
		std::atomic_flag is_drawing_;

		int processing_width_ = 0;
		std::unique_ptr<Shader> input_shader_;
		std::unique_ptr<Shader> output_shader_;
};

}
}
}

#endif /* ScanTarget_hpp */
