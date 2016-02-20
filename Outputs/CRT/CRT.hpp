//
//  CRT.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 19/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

#ifndef CRT_cpp
#define CRT_cpp

#include <stdint.h>
#include <stdarg.h>
#include <string>
#include <vector>
#include <mutex>

#include "Flywheel.hpp"

namespace Outputs {

struct Rect {
	struct {
		float x, y;
	} origin;

	struct {
		float width, height;
	} size;

	Rect() {}
	Rect(float x, float y, float width, float height) :
		origin({.x = x, .y = y}), size({.width = width, .height =height}) {}
};

class CRT {
	public:
		~CRT();

		enum DisplayType {
			PAL50,
			NTSC60
		};

		enum ColourSpace {
			YIQ,
			YUV
		};

		enum OutputDevice {
			Monitor,
			Television
		};

		/*!	Constructs the CRT with a specified clock rate, height and colour subcarrier frequency.
			The requested number of buffers, each with the requested number of bytes per pixel,
			is created for the machine to write raw pixel data to.

			@param cycles_per_line The clock rate at which this CRT will be driven, specified as the number
			of cycles expected to take up one whole scanline of the display.

			@param height_of_dispaly The number of lines that nominally form one field of the display, rounded
			up to the next whole integer.

			@param colour_cycle_numerator Specifies the numerator for the per-line frequency of the colour subcarrier.

			@param colour_cycle_denominator Specifies the denominator for the per-line frequency of the colour subcarrier.
			The colour subcarrier is taken to have colour_cycle_numerator/colour_cycle_denominator cycles per line.

			@param number_of_buffers The number of source data buffers to create for this machine. Machines
			may provide per-clock-cycle data in any form that they consider convenient, supplying a sampling
			function to convert between their data format and either a composite or RGB signal, allowing that
			work to be offloaded onto the GPU and allowing the output signal to be sampled at a rate appropriate
			to the display size.

			@param ... A list of sizes for source data buffers, provided as the number of bytes per sample.
			For compatibility with OpenGL ES, samples should be 1–4 bytes in size. If a machine requires more
			than 4 bytes/sample then it should use multiple buffers.

			@see @c set_rgb_sampling_function , @c set_composite_sampling_function
		*/
		CRT(unsigned int cycles_per_line, unsigned int height_of_display, ColourSpace colour_space, unsigned int colour_cycle_numerator, unsigned int colour_cycle_denominator, unsigned int number_of_buffers, ...);

		/*!	Constructs the CRT with the specified clock rate, with the display height and colour
			subcarrier frequency dictated by a standard display type and with the requested number of
			buffers, each with the requested number of bytes per pixel.

			Exactly identical to calling the designated constructor with colour subcarrier information
			looked up by display type.
		*/
		CRT(unsigned int cycles_per_line, DisplayType displayType, unsigned int number_of_buffers, ...);

		/*!	Resets the CRT with new timing information. The CRT then continues as though the new timing had
			been provided at construction. */
		void set_new_timing(unsigned int cycles_per_line, unsigned int height_of_display, ColourSpace colour_space, unsigned int colour_cycle_numerator, unsigned int colour_cycle_denominator);

		/*!	Resets the CRT with new timing information derived from a new display type. The CRT then continues
			as though the new timing had been provided at construction. */
		void set_new_display_type(unsigned int cycles_per_line, DisplayType displayType);

		/*!	Output at the sync level.

			@param number_of_cycles The amount of time to putput sync for.
		*/
		void output_sync(unsigned int number_of_cycles);

		/*!	Output at the blanking level.

			@param number_of_cycles The amount of time to putput the blanking level for.
		*/
		void output_blank(unsigned int number_of_cycles);

		/*!	Outputs the first written to the most-recently created run of data repeatedly for a prolonged period.

			@param number_of_cycles The number of cycles to repeat the output for.
		*/
		void output_level(unsigned int number_of_cycles);

		/*!	Declares that the caller has created a run of data via @c allocate_write_area and @c get_write_target_for_buffer
			that is at least @c number_of_cycles long, and that the first @c number_of_cycles/source_divider should be spread
			over that amount of time.

			@param number_of_cycles The amount of data to output.

			@param source_divider A divider for source data; if the divider is 1 then one source pixel is output every cycle,
			if it is 2 then one source pixel covers two cycles; if it is n then one source pixel covers n cycles.

			@see @c allocate_write_area , @c get_write_target_for_buffer
		*/
		void output_data(unsigned int number_of_cycles, unsigned int source_divider);

		/*!	Outputs a colour burst.

			@param number_of_cycles The length of the colour burst.

			@param phase The initial phase of the colour burst in a measuring system with 256 units
			per circle, e.g. 0 = 0 degrees, 128 = 180 degrees, 256 = 360 degree.

			@param magnitude The magnitude of the colour burst in 1/256ths of the magnitude of the
			positive portion of the wave.
		*/
		void output_colour_burst(unsigned int number_of_cycles, uint8_t phase, uint8_t magnitude);

		/*!	Ensures that the given number of output samples are allocated for writing.

			Following this call, the caller should call @c get_write_target_for_buffer for each
			buffer they requested to get the location of the allocated memory.

			The beginning of the most recently allocated area is used as the start
			of data written by a call to @c output_data; it is acceptable to write and to
			output less data than the amount requested but that may be less efficient.

			@param required_length The number of samples to allocate.
		*/
		void allocate_write_area(size_t required_length);

		/*!	Gets a pointer for writing to the area created by the most recent call to @c allocate_write_area
			for the nominated buffer.

			@param buffer The buffer to get a write target for.
		*/
		uint8_t *get_write_target_for_buffer(int buffer);

		/*!	Causes appropriate OpenGL or OpenGL ES calls to be issued in order to draw the current CRT state.
			The caller is responsible for ensuring that a valid OpenGL context exists for the duration of this call.
		*/
		void draw_frame(unsigned int output_width, unsigned int output_height, bool only_if_dirty);

		/*!	Tells the CRT that the next call to draw_frame will occur on a different OpenGL context than
			the previous.

			@param should_delete_resources If @c true then all resources — textures, vertex arrays, etc — 
			currently held by the CRT will be deleted now via calls to glDeleteTexture and equivalent. If
			@c false then the references are simply marked as invalid.
		*/
		void set_openGL_context_will_change(bool should_delete_resources);

		/*!	Sets a function that will map from whatever data the machine provided to a composite signal.

			@param shader A GLSL fragment including a function with the signature
			`float composite_sample(vec2 coordinate, float phase)` that evaluates to the composite signal
			level as a function of a source buffer sampling location and the provided colour carrier phase.
			The shader may assume a uniform array of sampler2Ds named `buffers` provides access to all input data.
		*/
		void set_composite_sampling_function(const char *shader);

		/*!	Sets a function that will map from whatever data the machine provided to an RGB signal.

			If the output mode is composite then a default mapping from RGB to the display's composite
			format will be applied.

			@param shader A GLSL fragent including a function with the signature
			`vec3 rgb_sample(vec2 coordinate)` that evaluates to an RGB colour as a function of
			the source buffer sampling location.
			The shader may assume a uniform array of sampler2Ds named `buffers` provides access to all input data.
		*/
		void set_rgb_sampling_function(const char *shader);

		/*!	Optionally sets a function that will map from an input cycle count to a colour carrier phase.

			If this function is not supplied then the colour phase is determined from
			the input clock rate and the the colour cycle clock rate. Machines whose per-line clock rate
			is not intended exactly to match the normal line time may prefer to supply a custom function.

			@param  A GLSL fragent including a function with the signature
			`float phase_for_clock_cycle(int cycle)` that returns the colour phase at the beginning of
			the supplied cycle.
		*/
//		void set_phase_function(const char *shader);

		void set_output_device(OutputDevice output_device);
		void set_visible_area(Rect visible_area)
		{
			_visible_area = visible_area;
		}

#ifdef DEBUG
		inline uint32_t get_field_cycle()
		{
			return _run_builders[_run_write_pointer]->duration / _time_multiplier;
		}
#endif

	private:
		CRT();
		void allocate_buffers(unsigned int number, va_list sizes);

		// the incoming clock lengths will be multiplied by something to give at least 1000
		// sample points per line
		unsigned int _time_multiplier;

		// fundamental creator-specified properties
		unsigned int _cycles_per_line;
		unsigned int _height_of_display;

		// colour invormation
		ColourSpace _colour_space;
		unsigned int _colour_cycle_numerator;
		unsigned int _colour_cycle_denominator;
		OutputDevice _output_device;

		// The user-supplied visible area
		Rect _visible_area;

		// the current scanning position (TODO: can I eliminate this in favour of just using the flywheels?)
		struct Vector {
			uint32_t x, y;
		} _rasterPosition, _scanSpeed[4], _beamWidth[4];

		// the two flywheels regulating scanning
		std::unique_ptr<Outputs::Flywheel> _horizontal_flywheel, _vertical_flywheel;

		// elements of sync separation
		bool _is_receiving_sync;				// true if the CRT is currently receiving sync (i.e. this is for edge triggering of horizontal sync)
		int _sync_capacitor_charge_level;		// this charges up during times of sync and depletes otherwise; needs to hit a required threshold to trigger a vertical sync
		int _sync_capacitor_charge_threshold;	// this charges up during times of sync and depletes otherwise; needs to hit a required threshold to trigger a vertical sync

		// the outer entry point for dispatching output_sync, output_blank, output_level and output_data
		enum Type {
			Sync, Level, Data, Blank, ColourBurst
		} type;
		void advance_cycles(unsigned int number_of_cycles, unsigned int source_divider, bool hsync_requested, bool vsync_requested, const bool vsync_charging, const Type type, uint16_t tex_x, uint16_t tex_y);

		// the inner entry point that determines whether and when the next sync event will occur within
		// the current output window
		Flywheel::SyncEvent get_next_vertical_sync_event(bool vsync_is_requested, unsigned int cycles_to_run_for, unsigned int *cycles_advanced);
		Flywheel::SyncEvent get_next_horizontal_sync_event(bool hsync_is_requested, unsigned int cycles_to_run_for, unsigned int *cycles_advanced);

		// each call to output_* generates a scan. A two-slot queue for scans allows edge extensions.
		struct Scan {
			Type type;
			unsigned int number_of_cycles;
			union {
				struct {
					unsigned int source_divider;
					uint16_t tex_x, tex_y;
				};
				struct {
					uint8_t phase, magnitude;
				};
			};
		} _scans[2];
		int _next_scan;
		void output_scan();

		struct CRTRunBuilder {
			// Resets the run builder.
			void reset();

			// Getter for new storage plus backing storage; in RGB mode input runs will map directly
			// from the input buffer to the screen. In composite mode input runs will map from the
			// input buffer to the processing buffer, and output runs will map from the processing
			// buffer to the screen.
			uint8_t *get_next_input_run();
			std::vector<uint8_t> _input_runs;

			uint8_t *get_next_output_run();
			std::vector<uint8_t> _output_runs;

			// Container for total length in cycles of all contained runs.
			uint32_t duration;

			// Storage for the length of run data uploaded so far; reset to zero by reset but otherwise
			// entrusted to the CRT to update.
			size_t uploaded_vertices;
			size_t number_of_vertices;
		};

		struct CRTInputBufferBuilder {
			CRTInputBufferBuilder(unsigned int number_of_buffers, va_list buffer_sizes);
			~CRTInputBufferBuilder();

			void allocate_write_area(size_t required_length);
			void reduce_previous_allocation_to(size_t actual_length);
			uint8_t *get_write_target_for_buffer(int buffer);

			// a pointer to the section of content buffer currently being
			// returned and to where the next section will begin
			uint16_t _next_write_x_position, _next_write_y_position;
			uint16_t _write_x_position, _write_y_position;
			size_t _write_target_pointer;
			size_t _last_allocation_amount;

			struct Buffer {
				uint8_t *data;
				size_t bytes_per_pixel;
			} *buffers;
			unsigned int number_of_buffers;

			// Storage for the amount of buffer uploaded so far; initialised correctly by the buffer
			// builder but otherwise entrusted to the CRT to update.
			unsigned int last_uploaded_line;
		};

		// the run and input data buffers
		std::unique_ptr<CRTInputBufferBuilder> _buffer_builder;
		CRTRunBuilder **_run_builders;
		int _run_write_pointer;
		std::shared_ptr<std::mutex> _output_mutex;

		// OpenGL state, kept behind an opaque pointer to avoid inclusion of the GL headers here.
		struct OpenGLState;
		OpenGLState *_openGL_state;

		// Other things the caller may have provided.
		char *_composite_shader;
		char *_rgb_shader;

		// Setup and teardown for the OpenGL code
		void construct_openGL();
		void destruct_openGL();

		// Methods used by the OpenGL code
		void prepare_shader();
		void prepare_vertex_array();
		void push_size_uniforms(unsigned int output_width, unsigned int output_height);

		char *get_vertex_shader();
		char *get_fragment_shader();
		char *get_compound_shader(const char *base, const char *insert);
};

}


#endif /* CRT_cpp */
