//
//  CRT.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 19/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

#ifndef CRT_hpp
#define CRT_hpp

#include <stdint.h>

#include "CRTTypes.hpp"
#include "Internals/Flywheel.hpp"
#include "Internals/CRTOpenGL.hpp"

namespace Outputs {
namespace CRT {

class CRT {
	private:
		CRT(unsigned int common_output_divisor);

		// the incoming clock lengths will be multiplied by something to give at least 1000
		// sample points per line
		unsigned int _time_multiplier;
		const unsigned int _common_output_divisor;

		// fundamental creator-specified properties
		unsigned int _cycles_per_line;
		unsigned int _height_of_display;

		// the two flywheels regulating scanning
		std::unique_ptr<Flywheel> _horizontal_flywheel, _vertical_flywheel;
		uint16_t _vertical_flywheel_output_divider;

		// elements of sync separation
		bool _is_receiving_sync;				// true if the CRT is currently receiving sync (i.e. this is for edge triggering of horizontal sync)
		int _sync_capacitor_charge_level;		// this charges up during times of sync and depletes otherwise; needs to hit a required threshold to trigger a vertical sync
		int _sync_capacitor_charge_threshold;	// this charges up during times of sync and depletes otherwise; needs to hit a required threshold to trigger a vertical sync
		unsigned int _sync_period;

		// each call to output_* generates a scan. A two-slot queue for scans allows edge extensions.
		struct Scan {
			enum Type {
				Sync, Level, Data, Blank, ColourBurst
			} type;
			unsigned int number_of_cycles;
			union {
				struct {
					unsigned int source_divider;
					uint16_t tex_x, tex_y;
				};
				struct {
					uint8_t phase, amplitude;
				};
			};
		};
		void output_scan(const Scan *scan);

		uint8_t _colour_burst_phase, _colour_burst_amplitude;
		uint16_t _colour_burst_time;
		bool _is_writing_composite_run;

		// the outer entry point for dispatching output_sync, output_blank, output_level and output_data
		void advance_cycles(unsigned int number_of_cycles, unsigned int source_divider, bool hsync_requested, bool vsync_requested, const bool vsync_charging, const Scan::Type type, uint16_t tex_x, uint16_t tex_y);

		// the inner entry point that determines whether and when the next sync event will occur within
		// the current output window
		Flywheel::SyncEvent get_next_vertical_sync_event(bool vsync_is_requested, unsigned int cycles_to_run_for, unsigned int *cycles_advanced);
		Flywheel::SyncEvent get_next_horizontal_sync_event(bool hsync_is_requested, unsigned int cycles_to_run_for, unsigned int *cycles_advanced);

		// OpenGL state, kept behind an opaque pointer to avoid inclusion of the GL headers here.
		std::unique_ptr<OpenGLOutputBuilder> _openGL_output_builder;

	public:
		/*!	Constructs the CRT with a specified clock rate, height and colour subcarrier frequency.
			The requested number of buffers, each with the requested number of bytes per pixel,
			is created for the machine to write raw pixel data to.

			@param cycles_per_line The clock rate at which this CRT will be driven, specified as the number
			of cycles expected to take up one whole scanline of the display.
			
			@param common_output_divisor The greatest a priori common divisor of all cycle counts that will be
			supplied to @c output_sync, @c output_data, etc; supply 1 if no greater divisor is known. For many
			machines output will run at a fixed multiple of the clock rate; knowing this divisor can improve
			internal precision.

			@param height_of_dispaly The number of lines that nominally form one field of the display, rounded
			up to the next whole integer.

			@param colour_cycle_numerator Specifies the numerator for the per-line frequency of the colour subcarrier.

			@param colour_cycle_denominator Specifies the denominator for the per-line frequency of the colour subcarrier.
			The colour subcarrier is taken to have colour_cycle_numerator/colour_cycle_denominator cycles per line.

			@param buffer_depth The depth per pixel of source data buffers to create for this machine. Machines
			may provide per-clock-cycle data in the depth that they consider convenient, supplying a sampling
			function to convert between their data format and either a composite or RGB signal, allowing that
			work to be offloaded onto the GPU and allowing the output signal to be sampled at a rate appropriate
			to the display size.

			@see @c set_rgb_sampling_function , @c set_composite_sampling_function
		*/
		CRT(unsigned int cycles_per_line, unsigned int common_output_divisor, unsigned int height_of_display, ColourSpace colour_space, unsigned int colour_cycle_numerator, unsigned int colour_cycle_denominator, unsigned int buffer_depth);

		/*!	Constructs the CRT with the specified clock rate, with the display height and colour
			subcarrier frequency dictated by a standard display type and with the requested number of
			buffers, each with the requested number of bytes per pixel.

			Exactly identical to calling the designated constructor with colour subcarrier information
			looked up by display type.
		*/
		CRT(unsigned int cycles_per_line, unsigned int common_output_divisor, DisplayType displayType, unsigned int buffer_depth);

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

			@param amplitude The amplitude of the colour burst in 1/256ths of the amplitude of the
			positive portion of the wave.
		*/
		void output_colour_burst(unsigned int number_of_cycles, uint8_t phase, uint8_t amplitude);

		/*!	Ensures that the given number of output samples are allocated for writing.

			The beginning of the most recently allocated area is used as the start
			of data written by a call to @c output_data; it is acceptable to write and to
			output less data than the amount requested but that may be less efficient.

			@param required_length The number of samples to allocate.
			@returns A pointer to the allocated area.
		*/
		inline uint8_t *allocate_write_area(size_t required_length)
		{
			return _openGL_output_builder->allocate_write_area(required_length);
		}

		/*!	Causes appropriate OpenGL or OpenGL ES calls to be issued in order to draw the current CRT state.
			The caller is responsible for ensuring that a valid OpenGL context exists for the duration of this call.
		*/
		inline void draw_frame(unsigned int output_width, unsigned int output_height, bool only_if_dirty)
		{
			_openGL_output_builder->draw_frame(output_width, output_height, only_if_dirty);
		}

		/*!	Tells the CRT that the next call to draw_frame will occur on a different OpenGL context than
			the previous.

			@param should_delete_resources If @c true then all resources — textures, vertex arrays, etc — 
			currently held by the CRT will be deleted now via calls to glDeleteTexture and equivalent. If
			@c false then the references are simply marked as invalid.
		*/
		inline void set_openGL_context_will_change(bool should_delete_resources)
		{
			_openGL_output_builder->set_openGL_context_will_change(should_delete_resources);
		}

		/*!	Sets a function that will map from whatever data the machine provided to a composite signal.

			@param shader A GLSL fragment including a function with the signature
			`float composite_sample(vec2 coordinate, float phase)` that evaluates to the composite signal
			level as a function of a source buffer sampling location and the provided colour carrier phase.
			The shader may assume a uniform array of sampler2Ds named `buffers` provides access to all input data.
		*/
		inline void set_composite_sampling_function(const char *shader)
		{
			_openGL_output_builder->set_composite_sampling_function(shader);
		}

		/*!	Sets a function that will map from whatever data the machine provided to an RGB signal.

			If the output mode is composite then a default mapping from RGB to the display's composite
			format will be applied.

			@param shader A GLSL fragent including a function with the signature
			`vec4 rgb_sample(vec2 coordinate)` that evaluates to an RGBA colour as a function of
			the source buffer sampling location.
			The shader may assume a uniform array of sampler2Ds named `buffers` provides access to all input data.
		*/
		inline void set_rgb_sampling_function(const char *shader)
		{
			_openGL_output_builder->set_rgb_sampling_function(shader);
		}

		/*!	Optionally sets a function that will map from an input cycle count to a colour carrier phase.

			If this function is not supplied then the colour phase is determined from
			the input clock rate and the the colour cycle clock rate. Machines whose per-line clock rate
			is not intended exactly to match the normal line time may prefer to supply a custom function.

			@param  A GLSL fragent including a function with the signature
			`float phase_for_clock_cycle(int cycle)` that returns the colour phase at the beginning of
			the supplied cycle.
		*/
//		void set_phase_function(const char *shader);

		inline void set_output_device(OutputDevice output_device)
		{
			_openGL_output_builder->set_output_device(output_device);
		}

		inline void set_visible_area(Rect visible_area)
		{
			_openGL_output_builder->set_visible_area(visible_area);
		}
};

}
}

#endif /* CRT_cpp */
