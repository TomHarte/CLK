//
//  CRTOpenGL.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 13/02/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#ifndef CRTOpenGL_h
#define CRTOpenGL_h

#include "../CRTTypes.hpp"
#include "CRTConstants.hpp"
#include "OpenGL.hpp"
#include "TextureTarget.hpp"
#include "Shaders/Shader.hpp"

#include "ArrayBuilder.hpp"
#include "TextureBuilder.hpp"

#include "Shaders/OutputShader.hpp"
#include "Shaders/IntermediateShader.hpp"
#include "Rectangle.hpp"

#include <mutex>
#include <vector>

namespace Outputs {
namespace CRT {

class OpenGLOutputBuilder {
	private:
		// colour information
		ColourSpace colour_space_;
		unsigned int colour_cycle_numerator_;
		unsigned int colour_cycle_denominator_;
		VideoSignal video_signal_;
		float gamma_;

		// timing information to allow reasoning about input information
		unsigned int input_frequency_;
		unsigned int cycles_per_line_;
		unsigned int height_of_display_;
		unsigned int horizontal_scan_period_;
		unsigned int vertical_scan_period_;
		unsigned int vertical_period_divider_;

		// The user-supplied visible area
		Rect visible_area_;

		// Other things the caller may have provided.
		std::string composite_shader_;
		std::string svideo_shader_;
		std::string rgb_shader_;
		GLint target_framebuffer_ = 0;

		// Methods used by the OpenGL code
		void prepare_output_shader();
		void prepare_rgb_input_shaders();
		void prepare_svideo_input_shaders();
		void prepare_composite_input_shaders();

		void prepare_output_vertex_array();
		void prepare_source_vertex_array();

		// the run and input data buffers
		std::mutex output_mutex_;
		std::mutex draw_mutex_;

		// transient buffers indicating composite data not yet decoded
		GLsizei composite_src_output_y_;

		std::unique_ptr<OpenGL::OutputShader> output_shader_program_;

		std::unique_ptr<OpenGL::IntermediateShader> composite_input_shader_program_;
		std::unique_ptr<OpenGL::IntermediateShader> composite_separation_filter_program_;
		std::unique_ptr<OpenGL::IntermediateShader> composite_chrominance_filter_shader_program_;

		std::unique_ptr<OpenGL::IntermediateShader> svideo_input_shader_program_;

		std::unique_ptr<OpenGL::IntermediateShader> rgb_input_shader_program_;
		std::unique_ptr<OpenGL::IntermediateShader> rgb_filter_shader_program_;

		std::unique_ptr<OpenGL::TextureTarget> composite_texture_;	// receives raw composite levels
		std::unique_ptr<OpenGL::TextureTarget> separated_texture_;	// receives filtered Y in the R channel plus unfiltered but demodulated chrominance in G and B
		std::unique_ptr<OpenGL::TextureTarget> filtered_texture_;	// receives filtered YIQ or YUV

		std::unique_ptr<OpenGL::TextureTarget> work_texture_;		// used for all intermediate rendering if texture fences are supported

		std::unique_ptr<OpenGL::TextureTarget> framebuffer_;		// the current pixel output

		GLuint output_vertex_array_;
		GLuint source_vertex_array_;

		unsigned int last_output_width_, last_output_height_;

		void set_timing_uniforms();
		void set_colour_space_uniforms();
		void set_gamma();

		void establish_OpenGL_state();
		void reset_all_OpenGL_state();

		GLsync fence_;
		float get_composite_output_width() const;
		void set_output_shader_width();

		float integer_coordinate_multiplier_ = 1.0f;

		// Maintain a couple of rectangles for masking off the extreme edge of the display;
		// this is a bit of a cheat: there's some tolerance in when a sync pulse will be
		// generated. So it might be slightly later than expected. Which might cause a scan
		// that is slightly longer than expected. Which means that from then on, those scans
		// might have touched parts of the extreme edge of the display which are not rescanned.
		// Which because I've implemented persistence-of-vision as an in-buffer effect will
		// cause perpetual persistence.
		//
		// The fix: just always treat that area as invisible. This is acceptable thanks to
		// the concept of overscan. One is allowed not to display extreme ends of the image.
		std::unique_ptr<OpenGL::Rectangle> right_overlay_;
		std::unique_ptr<OpenGL::Rectangle> left_overlay_;

	public:
		// These two are protected by output_mutex_.
		TextureBuilder texture_builder;
		ArrayBuilder array_builder;

		OpenGLOutputBuilder(std::size_t bytes_per_pixel);
		~OpenGLOutputBuilder();

		inline void set_colour_format(ColourSpace colour_space, unsigned int colour_cycle_numerator, unsigned int colour_cycle_denominator) {
			std::lock_guard<std::mutex> output_guard(output_mutex_);
			colour_space_ = colour_space;
			colour_cycle_numerator_ = colour_cycle_numerator;
			colour_cycle_denominator_ = colour_cycle_denominator;
			set_colour_space_uniforms();
		}

		inline void set_visible_area(Rect visible_area) {
			visible_area_ = visible_area;
		}

		inline void set_gamma(float gamma) {
			gamma_ = gamma;
			set_gamma();
		}

		inline std::unique_lock<std::mutex> get_output_lock() {
			return std::unique_lock<std::mutex>(output_mutex_);
		}

		inline VideoSignal get_output_device() {
			return video_signal_;
		}

		inline uint16_t get_composite_output_y() {
			return static_cast<uint16_t>(composite_src_output_y_);
		}

		inline bool composite_output_buffer_is_full() {
			return composite_src_output_y_ == IntermediateBufferHeight;
		}

		inline void increment_composite_output_y() {
			if(!composite_output_buffer_is_full())
				composite_src_output_y_++;
		}

		void set_target_framebuffer(GLint);
		void draw_frame(unsigned int output_width, unsigned int output_height, bool only_if_dirty);
		void set_openGL_context_will_change(bool should_delete_resources);
		void set_composite_sampling_function(const std::string &);
		void set_svideo_sampling_function(const std::string &);
		void set_rgb_sampling_function(const std::string &);
		void set_video_signal(VideoSignal);
		void set_timing(unsigned int input_frequency, unsigned int cycles_per_line, unsigned int height_of_display, unsigned int horizontal_scan_period, unsigned int vertical_scan_period, unsigned int vertical_period_divider);
		void set_integer_coordinate_multiplier(float multiplier);
};

}
}

#endif /* CRTOpenGL_h */
