//
//  CRTOpenGL.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 13/02/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef CRTOpenGL_h
#define CRTOpenGL_h

#include "../CRTTypes.hpp"
#include "CRTConstants.hpp"
#include "OpenGL.hpp"
#include "TextureTarget.hpp"
#include "Shader.hpp"
#include "TextureBuilder.hpp"

#include "Shaders/OutputShader.hpp"
#include "Shaders/IntermediateShader.hpp"

#include <mutex>
#include <vector>

namespace Outputs {
namespace CRT {

class OpenGLOutputBuilder {
	private:
		// colour information
		ColourSpace _colour_space;
		unsigned int _colour_cycle_numerator;
		unsigned int _colour_cycle_denominator;
		OutputDevice _output_device;

		// timing information to allow reasoning about input information
		unsigned int _input_frequency;
		unsigned int _cycles_per_line;
		unsigned int _height_of_display;
		unsigned int _horizontal_scan_period;
		unsigned int _vertical_scan_period;
		unsigned int _vertical_period_divider;

		// The user-supplied visible area
		Rect _visible_area;

		// Other things the caller may have provided.
		char *_composite_shader;
		char *_rgb_shader;

		// Methods used by the OpenGL code
		void prepare_output_shader();
		void prepare_rgb_input_shaders();
		void prepare_composite_input_shaders();

		void prepare_output_vertex_array();
		void prepare_source_vertex_array();

		// the run and input data buffers
		std::unique_ptr<TextureBuilder> _texture_builder;
		std::unique_ptr<std::mutex> _output_mutex;
		std::unique_ptr<std::mutex> _draw_mutex;

		// transient buffers indicating composite data not yet decoded
		GLsizei _composite_src_output_y;

		std::unique_ptr<OpenGL::OutputShader> output_shader_program;
		std::unique_ptr<OpenGL::IntermediateShader> composite_input_shader_program, composite_separation_filter_program, composite_y_filter_shader_program, composite_chrominance_filter_shader_program;
		std::unique_ptr<OpenGL::IntermediateShader> rgb_input_shader_program, rgb_filter_shader_program;

		std::unique_ptr<OpenGL::TextureTarget> compositeTexture;	// receives raw composite levels
		std::unique_ptr<OpenGL::TextureTarget> separatedTexture;	// receives unfiltered Y in the R channel plus unfiltered but demodulated chrominance in G and B
		std::unique_ptr<OpenGL::TextureTarget> filteredYTexture;	// receives filtered Y in the R channel plus unfiltered chrominance in G and B
		std::unique_ptr<OpenGL::TextureTarget> filteredTexture;		// receives filtered YIQ or YUV

		std::unique_ptr<OpenGL::TextureTarget> framebuffer;			// the current pixel output

		GLuint output_array_buffer, output_vertex_array;
		GLuint source_array_buffer, source_vertex_array;

		unsigned int _last_output_width, _last_output_height;

		GLuint shadowMaskTextureName;

		GLuint defaultFramebuffer;

		void set_timing_uniforms();
		void set_colour_space_uniforms();

		void establish_OpenGL_state();
		void reset_all_OpenGL_state();

	public:
		OpenGLOutputBuilder(unsigned int buffer_depth);
		~OpenGLOutputBuilder();

		inline uint8_t *get_next_source_run()
		{
			if(_line_buffer.data.size() < _line_buffer.pointer + SourceVertexSize)
				_line_buffer.data.resize(_line_buffer.pointer + SourceVertexSize);
			return &_line_buffer.data[_line_buffer.pointer];
		}

		inline void complete_source_run()
		{
			_line_buffer.pointer += SourceVertexSize;
		}

		inline uint8_t *get_buffered_source_runs(size_t &size)
		{
			size = _line_buffer.pointer;
			return _line_buffer.data.data();
		}

		inline uint8_t *get_next_output_run()
		{
			if(_output_buffer.pointer == OutputVertexBufferDataSize) return nullptr;
			return &_output_buffer.data[_output_buffer.pointer];
		}

		inline void complete_output_run()
		{
			size_t line_buffer_size = _line_buffer.data.size();
			if(_source_buffer.pointer + line_buffer_size < SourceVertexBufferDataSize)
			{
				_output_buffer.pointer += OutputVertexSize;
				memcpy(&_source_buffer.data[_source_buffer.pointer], _line_buffer.data.data(), _line_buffer.data.size());
				_source_buffer.pointer += _line_buffer.data.size();
				_line_buffer.pointer = 0;
			}
		}

		inline void set_colour_format(ColourSpace colour_space, unsigned int colour_cycle_numerator, unsigned int colour_cycle_denominator)
		{
			_output_mutex->lock();
			_colour_space = colour_space;
			_colour_cycle_numerator = colour_cycle_numerator;
			_colour_cycle_denominator = colour_cycle_denominator;
			set_colour_space_uniforms();
			_output_mutex->unlock();
		}

		inline void set_visible_area(Rect visible_area)
		{
			_visible_area = visible_area;
		}

		inline bool composite_output_run_has_room_for_vertex()
		{
			return _output_buffer.pointer < OutputVertexBufferDataSize;
		}

		inline void lock_output()
		{
			_output_mutex->lock();
		}

		inline void unlock_output()
		{
			_output_mutex->unlock();
		}

		inline OutputDevice get_output_device()
		{
			return _output_device;
		}

		inline uint16_t get_composite_output_y()
		{
			return (uint16_t)_composite_src_output_y;
		}

		inline bool composite_output_buffer_is_full()
		{
			return _composite_src_output_y == IntermediateBufferHeight;
		}

		inline void increment_composite_output_y()
		{
			if(!composite_output_buffer_is_full())
				_composite_src_output_y++;
		}

		inline uint8_t *allocate_write_area(size_t required_length)
		{
			return _texture_builder->allocate_write_area(required_length);
		}

		inline void reduce_previous_allocation_to(size_t actual_length)
		{
			_texture_builder->reduce_previous_allocation_to(actual_length);
		}

		inline bool input_buffer_is_full()
		{
			return _texture_builder->is_full();
		}

		inline uint16_t get_last_write_x_posititon()
		{
			return _texture_builder->get_last_write_x_position();
		}

		inline uint16_t get_last_write_y_posititon()
		{
			return _texture_builder->get_last_write_y_position();
		}

		void draw_frame(unsigned int output_width, unsigned int output_height, bool only_if_dirty);
		void set_openGL_context_will_change(bool should_delete_resources);
		void set_composite_sampling_function(const char *shader);
		void set_rgb_sampling_function(const char *shader);
		void set_output_device(OutputDevice output_device);
		void set_timing(unsigned int input_frequency, unsigned int cycles_per_line, unsigned int height_of_display, unsigned int horizontal_scan_period, unsigned int vertical_scan_period, unsigned int vertical_period_divider);

		struct Buffer {
			std::vector<uint8_t> data;
			size_t pointer;
			Buffer() : pointer(0) {}
		} _line_buffer, _source_buffer, _output_buffer;

		GLsync _fence;
};

}
}

#endif /* CRTOpenGL_h */
