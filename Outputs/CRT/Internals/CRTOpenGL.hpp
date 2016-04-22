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
#include "CRTInputBufferBuilder.hpp"
#include "CRTRunBuilder.hpp"

#include <mutex>

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
		void prepare_rgb_output_shader();
		void prepare_composite_output_shader();
		std::unique_ptr<OpenGL::Shader> prepare_output_shader(char *vertex_shader, char *fragment_shader, GLint source_texture_unit);

		void prepare_composite_input_shader();
		std::unique_ptr<OpenGL::Shader> prepare_intermediate_shader(const char *input_position, const char *header, char *fragment_shader, GLenum texture_unit, bool extends);

		void prepare_output_vertex_array();
		void prepare_source_vertex_array();
		void push_size_uniforms(unsigned int output_width, unsigned int output_height);

		// the run and input data buffers
		std::unique_ptr<CRTInputBufferBuilder> _buffer_builder;
		CRTRunBuilder **_run_builders;
		int _run_write_pointer;
		std::shared_ptr<std::mutex> _output_mutex;

		// transient buffers indicating composite data not yet decoded
		uint16_t _composite_src_output_y, _cleared_composite_output_y;

		char *get_output_vertex_shader(const char *header);
		char *get_rgb_output_vertex_shader();
		char *get_composite_output_vertex_shader();

		char *get_output_fragment_shader(const char *sampling_function, const char *header, const char *fragColour_function);
		char *get_rgb_output_fragment_shader();
		char *get_composite_output_fragment_shader();

		char *get_input_vertex_shader(const char *input_position, const char *header);
		char *get_input_fragment_shader();
		char *get_y_filter_fragment_shader();

		std::unique_ptr<OpenGL::Shader> rgb_shader_program;
		std::unique_ptr<OpenGL::Shader> composite_input_shader_program, composite_y_filter_shader_program, composite_output_shader_program;

		GLuint output_array_buffer, output_vertex_array;
		GLuint source_array_buffer, source_vertex_array;

		GLint windowSizeUniform, timestampBaseUniform;
		GLint boundsOriginUniform, boundsSizeUniform;

		GLuint textureName, shadowMaskTextureName;

		GLuint defaultFramebuffer;

		std::unique_ptr<OpenGL::TextureTarget> compositeTexture;	// receives raw composite levels
		std::unique_ptr<OpenGL::TextureTarget> filteredYTexture;	// receives filtered Y in the R channel plus unfiltered I/U and Q/V in G and B
		std::unique_ptr<OpenGL::TextureTarget> filteredTexture;		// receives filtered YIQ or YUV

		void perform_output_stage(unsigned int output_width, unsigned int output_height, OpenGL::Shader *const shader);

	public:
		OpenGLOutputBuilder(unsigned int buffer_depth);
		~OpenGLOutputBuilder();

		inline void set_colour_format(ColourSpace colour_space, unsigned int colour_cycle_numerator, unsigned int colour_cycle_denominator)
		{
			_colour_space = colour_space;
			_colour_cycle_numerator = colour_cycle_numerator;
			_colour_cycle_denominator = colour_cycle_denominator;
		}

		inline void set_visible_area(Rect visible_area)
		{
			_visible_area = visible_area;
		}

		inline uint8_t *get_next_source_run()
		{
			_output_mutex->lock();
			return &_source_buffer_data[_source_buffer_data_pointer % SourceVertexBufferDataSize];
		}

		inline void complete_source_run()
		{
			_source_buffer_data_pointer += 2 * SourceVertexSize;
			_output_mutex->unlock();
		}

		inline uint8_t *get_next_output_run()
		{
			_output_mutex->lock();
			return &_output_buffer_data[_output_buffer_data_pointer];
		}

		inline void complete_output_run(GLsizei vertices_written)
		{
			_run_builders[_run_write_pointer]->amount_of_data += (size_t)(vertices_written * OutputVertexSize);
			_output_buffer_data_pointer = (_output_buffer_data_pointer + vertices_written * OutputVertexSize) % OutputVertexBufferDataSize;
			_output_mutex->unlock();
		}

		inline OutputDevice get_output_device()
		{
			return _output_device;
		}

		inline uint32_t get_current_field_time()
		{
			return _run_builders[_run_write_pointer]->duration;
		}

		inline void add_to_field_time(uint32_t amount)
		{
			_run_builders[_run_write_pointer]->duration += amount;
		}

		inline uint16_t get_composite_output_y()
		{
			return _composite_src_output_y % IntermediateBufferHeight;
		}

		inline void increment_composite_output_y()
		{
			_composite_src_output_y++;
		}

		inline void increment_field()
		{
			_output_mutex->lock();
			_run_write_pointer = (_run_write_pointer + 1)%NumberOfFields;
			_run_builders[_run_write_pointer]->start = (size_t)_output_buffer_data_pointer;
			_run_builders[_run_write_pointer]->reset();
			_output_mutex->unlock();
		}

		inline int get_current_field()
		{
			return _run_write_pointer;
		}

		inline uint8_t *allocate_write_area(size_t required_length)
		{
			_output_mutex->lock();
			_buffer_builder->allocate_write_area(required_length);
			uint8_t *output = _input_texture_data ? _buffer_builder->get_write_target(_input_texture_data) : nullptr;
			_output_mutex->unlock();
			return output;
		}

		inline void reduce_previous_allocation_to(size_t actual_length)
		{
			_buffer_builder->reduce_previous_allocation_to(actual_length, _input_texture_data);
		}

		inline uint16_t get_last_write_x_posiiton()
		{
			return _buffer_builder->_write_x_position;
		}

		inline uint16_t get_last_write_y_posiiton()
		{
			return _buffer_builder->_write_y_position;
		}

		void draw_frame(unsigned int output_width, unsigned int output_height, bool only_if_dirty);
		void set_openGL_context_will_change(bool should_delete_resources);
		void set_composite_sampling_function(const char *shader);
		void set_rgb_sampling_function(const char *shader);
		void set_output_device(OutputDevice output_device);
		inline void set_timing(unsigned int cycles_per_line, unsigned int height_of_display, unsigned int horizontal_scan_period, unsigned int vertical_scan_period, unsigned int vertical_period_divider)
		{
			_cycles_per_line = cycles_per_line;
			_height_of_display = height_of_display;
			_horizontal_scan_period = horizontal_scan_period;
			_vertical_scan_period = vertical_scan_period;
			_vertical_period_divider = vertical_period_divider;

			// TODO: update related uniforms
		}

		uint8_t *_input_texture_data;
		GLuint _input_texture_array;
		GLsync _input_texture_sync;
		GLsizeiptr _input_texture_array_size;

		uint8_t *_output_buffer_data;
		GLsizei _output_buffer_data_pointer;

		uint8_t *_source_buffer_data;
		GLsizei _source_buffer_data_pointer;
		GLsizei _drawn_source_buffer_data_pointer;
};

}
}

#endif /* CRTOpenGL_h */
