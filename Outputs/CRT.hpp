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

#include "CRTFrame.h"

namespace Outputs {

class CRT {
	public:
		~CRT();

		enum DisplayType {
			PAL50,
			NTSC60
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
		CRT(unsigned int cycles_per_line, unsigned int height_of_display, unsigned int colour_cycle_numerator, unsigned int colour_cycle_denominator, unsigned int number_of_buffers, ...);

		/*!	Constructs the CRT with the specified clock rate, with the display height and colour
			subcarrier frequency dictated by a standard display type and with the requested number of
			buffers, each with the requested number of bytes per pixel.

			Exactly identical to calling the designated constructor with colour subcarrier information
			looked up by display type.
		*/
		CRT(unsigned int cycles_per_line, DisplayType displayType, unsigned int number_of_buffers, ...);

		/*!	Resets the CRT with new timing information. The CRT then continues as though the new timing had
			been provided at construction. */
		void set_new_timing(unsigned int cycles_per_line, unsigned int height_of_display, unsigned int colour_cycle_numerator, unsigned int colour_cycle_denominator);

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
		void allocate_write_area(int required_length);

		/*!	Gets a pointer for writing to the area created by the most recent call to @c allocate_write_area
			for the nominated buffer.

			@param buffer The buffer to get a write target for.
		*/
		uint8_t *get_write_target_for_buffer(int buffer);

		// MARK: Binding
		class Delegate {
			public:
				/*!	Notifies the delegate that a new frame is complete, providing an ID for it.

					The delegate then owns that frame. It can draw it by issuing an @c draw_frame
					call to the CRT. It should call @c return_frame to return the oldest frame it
					is currently holding.

					@param crt The CRT that produced the new frame.
					@param frame_id An identifier for the finished frame, used for calls to @c draw_frame.
					@param did_detect_vsync @c true if this frame ended due to a vsync signal detected in
					the incoming signal; @c false otherwise.
				*/
				virtual void crt_did_end_frame(CRT *crt, int frame_id, bool did_detect_vsync) = 0;
		};

		/*!	Sets the CRT frame delegate. The delegate will be notified as frames are completed; it is
			responsible for requesting that they be drawn and returning them when they are no longer needed.

			@param delegate The delegate.
		*/
		void set_delegate(std::weak_ptr<Delegate> delegate);

		/*!	Causes appropriate OpenGL or OpenGL ES calls to be made in order to draw a frame. The caller
			is responsible for ensuring that a valid OpenGL context exists for the duration of this call.

			@param frame_id The frame to draw.
		*/
		void draw_frame(int frame_id);

		/*!	Indicates that the delegate has no further interest in the oldest frame posted to it. */
		void return_frame();

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
		void set_phase_function(const char *shader);

	private:
		CRT();
		void allocate_buffers(unsigned int number, va_list sizes);

		// the incoming clock lengths will be multiplied by something to give at least 1000
		// sample points per line
		unsigned int _time_multiplier;

		// fundamental creator-specified properties
		unsigned int _cycles_per_line;
		unsigned int _height_of_display;

		// properties directly derived from there
		unsigned int _hsync_error_window;			// the permitted window around the expected sync position in which a sync pulse will be recognised; calculated once at init

		// the current scanning position
		struct Vector {
			uint32_t x, y;
		} _rasterPosition, _scanSpeed[4], _beamWidth[4];

		// outer elements of sync separation
		bool _is_receiving_sync;				// true if the CRT is currently receiving sync (i.e. this is for edge triggering of horizontal sync)
		bool _did_detect_hsync;					// true if horizontal sync was detected during this scanline (so, this affects flywheel adjustments)
		int _sync_capacitor_charge_level;		// this charges up during times of sync and depletes otherwise; needs to hit a required threshold to trigger a vertical sync
		int _sync_capacitor_charge_threshold;	// this charges up during times of sync and depletes otherwise; needs to hit a required threshold to trigger a vertical sync
		int _is_in_vsync;

		// components of the flywheel sync
		unsigned int _horizontal_counter;			// time run since the _start_ of the last horizontal sync
		unsigned int _expected_next_hsync;			// our current expection of when the next horizontal sync will be encountered (which implies current flywheel velocity)
		unsigned int _horizontal_retrace_time;
		bool _is_in_hsync;					// true for the duration of a horizontal sync — used to determine beam running direction and speed
		bool _did_detect_vsync;				// true if vertical sync was detected in the input stream rather than forced by emergency measure

		// the outer entry point for dispatching output_sync, output_blank, output_level and output_data
		enum Type {
			Sync, Level, Data, Blank, ColourBurst
		} type;
		void advance_cycles(unsigned int number_of_cycles, unsigned int source_divider, bool hsync_requested, bool vsync_requested, const bool vsync_charging, const Type type, uint16_t tex_x, uint16_t tex_y);

		// the inner entry point that determines whether and when the next sync event will occur within
		// the current output window
		enum SyncEvent {
			None,
			StartHSync, EndHSync,
			StartVSync, EndVSync
		};
		SyncEvent get_next_vertical_sync_event(bool vsync_is_requested, unsigned int cycles_to_run_for, unsigned int *cycles_advanced);
		SyncEvent get_next_horizontal_sync_event(bool hsync_is_requested, unsigned int cycles_to_run_for, unsigned int *cycles_advanced);

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

		// MARK: shader storage and information.
		/*!	Gets the vertex shader for display of vended CRTFrames.

			@returns A vertex shader, allocated using a C function. The caller then owns the memory
			and is responsible for free'ing it.
		*/
		char *get_vertex_shader();

		/*!	Gets a fragment shader for display of vended CRTFrames based on the supplied sampling function.

			@param sample_function A GLSL fragment including a function with the signature 
			`float sample(vec2 coordinate, float phase)` that evaluates to the composite signal level
			as a function of a source buffer sampling location and the current colour carrier phase.

			@returns A complete fragment shader.
		*/
		char *get_fragment_shader(const char *sample_function);

		/*!	Gets a fragment shader for composite display of vended CRTFrames based on a default encoding
			of the supplied sampling function.

			@param sample_function A GLSL fragent including a function with the signature
			`vec3 rgb_sample(vec2 coordinate)` that evaluates to an RGB colour as a function of
			the source buffer sampling location.

			@returns A complete fragment shader.
		*/
		char *get_rgb_encoding_fragment_shader(const char *sample_function);

		struct CRTFrameBuilder {
			CRTFrame frame;

			CRTFrameBuilder(uint16_t width, uint16_t height, unsigned int number_of_buffers, va_list buffer_sizes);
			~CRTFrameBuilder();

			private:
				std::vector<uint8_t> _all_runs;

				void reset();
				void complete();

				uint8_t *get_next_run();
				friend CRT;

				void allocate_write_area(int required_length);
				uint8_t *get_write_target_for_buffer(int buffer);

				// a pointer to the section of content buffer currently being
				// returned and to where the next section will begin
				uint16_t _next_write_x_position, _next_write_y_position;
				uint16_t _write_x_position, _write_y_position;
				size_t _write_target_pointer;
		};

		static const int kCRTNumberOfFrames = 4;

		// the run delegate and the triple buffer
		CRTFrameBuilder *_frame_builders[kCRTNumberOfFrames];
		CRTFrameBuilder *_current_frame_builder;
		int _frames_with_delegate;
		int _frame_read_pointer;
		Delegate *_delegate;
};

}


#endif /* CRT_cpp */
