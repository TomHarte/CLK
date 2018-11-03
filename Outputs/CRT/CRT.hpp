//
//  CRT.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 19/07/2015.
//  Copyright 2015 Thomas Harte. All rights reserved.
//

#ifndef CRT_hpp
#define CRT_hpp

#include <cstdint>

#include "CRTTypes.hpp"
#include "Internals/Flywheel.hpp"
#include "Internals/CRTOpenGL.hpp"
#include "Internals/ArrayBuilder.hpp"
#include "Internals/TextureBuilder.hpp"

#include "ScanTarget.hpp"

namespace Outputs {
namespace CRT {

class CRT;

class Delegate {
	public:
		virtual void crt_did_end_batch_of_frames(CRT *crt, int number_of_frames, int number_of_unexpected_vertical_syncs) = 0;
};

class CRT {
	private:
		CRT(int common_output_divisor, int buffer_depth);

		// the incoming clock lengths will be multiplied by something to give at least 1000
		// sample points per line
		int time_multiplier_ = 1;
		const int common_output_divisor_ = 1;

		// the two flywheels regulating scanning
		std::unique_ptr<Flywheel> horizontal_flywheel_, vertical_flywheel_;
		uint16_t vertical_flywheel_output_divider_ = 1;

		struct Scan {
			enum Type {
				Sync, Level, Data, Blank, ColourBurst
			} type;
			int number_of_cycles, number_of_samples;
			union {
				struct {
					uint8_t phase, amplitude;
				};
			};
		};
		void output_scan(const Scan *scan);

		int16_t colour_burst_angle_ = 0;
		uint8_t colour_burst_amplitude_ = 30;
		int colour_burst_phase_adjustment_ = 0;
		bool is_writing_composite_run_ = false;

		int phase_denominator_ = 1, phase_numerator_ = 1, colour_cycle_numerator_ = 1;
		bool is_alernate_line_ = false, phase_alternates_ = false;

		// the outer entry point for dispatching output_sync, output_blank, output_level and output_data
		void advance_cycles(int number_of_cycles, bool hsync_requested, bool vsync_requested, const Scan::Type type, int number_of_samples);

		// the inner entry point that determines whether and when the next sync event will occur within
		// the current output window
		Flywheel::SyncEvent get_next_vertical_sync_event(bool vsync_is_requested, int cycles_to_run_for, int *cycles_advanced);
		Flywheel::SyncEvent get_next_horizontal_sync_event(bool hsync_is_requested, int cycles_to_run_for, int *cycles_advanced);

		// the delegate
		Delegate *delegate_ = nullptr;
		int frames_since_last_delegate_call_ = 0;

		// sync counter, for determining vertical sync
		bool is_receiving_sync_ = false;					// true if the CRT is currently receiving sync (i.e. this is for edge triggering of horizontal sync)
		bool is_accumulating_sync_ = false;					// true if a sync level has triggered the suspicion that a vertical sync might be in progress
		bool is_refusing_sync_ = false;						// true once a vertical sync has been detected, until a prolonged period of non-sync has ended suspicion of an ongoing vertical sync
		int sync_capacitor_charge_threshold_ = 0;	// this charges up during times of sync and depletes otherwise; needs to hit a required threshold to trigger a vertical sync
		int cycles_of_sync_ = 0;					// the number of cycles since the potential vertical sync began
		int cycles_since_sync_ = 0;				// the number of cycles since last in sync, for defeating the possibility of this being a vertical sync

		int cycles_per_line_ = 1;

		ScanTarget *scan_target_ = nullptr;

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

			@param height_of_display The number of lines that nominally form one field of the display, rounded
			up to the next whole integer.

			@param colour_cycle_numerator Specifies the numerator for the per-line frequency of the colour subcarrier.

			@param colour_cycle_denominator Specifies the denominator for the per-line frequency of the colour subcarrier.
			The colour subcarrier is taken to have colour_cycle_numerator/colour_cycle_denominator cycles per line.

			@param vertical_sync_half_lines The expected length of vertical synchronisation (equalisation pulses aside),
			in multiples of half a line.

			@param buffer_depth The depth per pixel of source data buffers to create for this machine. Machines
			may provide per-clock-cycle data in the depth that they consider convenient, supplying a sampling
			function to convert between their data format and either a composite or RGB signal, allowing that
			work to be offloaded onto the GPU and allowing the output signal to be sampled at a rate appropriate
			to the display size.

			@see @c set_rgb_sampling_function , @c set_composite_sampling_function
		*/
		CRT(int cycles_per_line,
			int common_output_divisor,
			int height_of_display,
			ColourSpace colour_space,
			int colour_cycle_numerator, int colour_cycle_denominator,
			int vertical_sync_half_lines,
			bool should_alternate,
			int buffer_depth);

		/*!	Constructs the CRT with the specified clock rate, with the display height and colour
			subcarrier frequency dictated by a standard display type and with the requested number of
			buffers, each with the requested number of bytes per pixel.

			Exactly identical to calling the designated constructor with colour subcarrier information
			looked up by display type.
		*/
		CRT(int cycles_per_line,
			int common_output_divisor,
			DisplayType displayType,
			int buffer_depth);

		/*!	Resets the CRT with new timing information. The CRT then continues as though the new timing had
			been provided at construction. */
		void set_new_timing(int cycles_per_line, int height_of_display, ColourSpace colour_space, int colour_cycle_numerator, int colour_cycle_denominator, int vertical_sync_half_lines, bool should_alternate);

		/*!	Resets the CRT with new timing information derived from a new display type. The CRT then continues
			as though the new timing had been provided at construction. */
		void set_new_display_type(int cycles_per_line, DisplayType displayType);

		/*!	Output at the sync level.

			@param number_of_cycles The amount of time to putput sync for.
		*/
		void output_sync(int number_of_cycles);

		/*!	Output at the blanking level.

			@param number_of_cycles The amount of time to putput the blanking level for.
		*/
		void output_blank(int number_of_cycles);

		/*!	Outputs the first written to the most-recently created run of data repeatedly for a prolonged period.

			@param number_of_cycles The number of cycles to repeat the output for.
		*/
		void output_level(int number_of_cycles);

		/*!	Declares that the caller has created a run of data via @c allocate_write_area and @c get_write_target_for_buffer
			that is at least @c number_of_samples long, and that the first @c number_of_samples should be spread
			over @c number_of_cycles.

			@param number_of_cycles The amount of data to output.

			@param number_of_samples The number of samples of input data to output.

			@see @c allocate_write_area , @c get_write_target_for_buffer
		*/
		void output_data(int number_of_cycles, size_t number_of_samples);

		/*! A shorthand form for output_data that assumes the number of cycles to output for is the same as the number of samples. */
		void output_data(int number_of_cycles) {
			output_data(number_of_cycles, size_t(number_of_cycles));
		}

		/*!	Outputs a colour burst.

			@param number_of_cycles The length of the colour burst.

			@param phase The initial phase of the colour burst in a measuring system with 256 units
			per circle, e.g. 0 = 0 degrees, 128 = 180 degrees, 256 = 360 degree.

			@param amplitude The amplitude of the colour burst in 1/256ths of the amplitude of the
			positive portion of the wave.
		*/
		void output_colour_burst(int number_of_cycles, uint8_t phase, uint8_t amplitude = 102);

		/*! Outputs a colour burst exactly in phase with CRT expectations using the idiomatic amplitude.

			@param number_of_cycles The length of the colour burst;
		*/
		void output_default_colour_burst(int number_of_cycles);

		/*! Sets the current phase of the colour subcarrier used by output_default_colour_burst.

			@param phase The normalised instantaneous phase. 0.0f is the start of a colour cycle, 1.0f is the
			end of a colour cycle, 0.25f is a quarter of the way through a colour cycle, etc.
		*/
		void set_immediate_default_phase(float phase);

		/*!	Attempts to allocate the given number of output samples for writing.

			The beginning of the most recently allocated area is used as the start
			of data written by a call to @c output_data; it is acceptable to write and to
			output less data than the amount requested but that may be less efficient.

			Allocation should fail only if emulation is running significantly below real speed.

			@param required_length The number of samples to allocate.
			@returns A pointer to the allocated area if room is available; @c nullptr otherwise.
		*/
		inline uint8_t *allocate_write_area(std::size_t required_length, std::size_t required_alignment = 1) {
			return scan_target_->allocate_write_area(required_length, required_alignment);
		}

		/*!	Sets the gamma exponent for the simulated screen. */
		void set_input_gamma(float gamma);

		enum CompositeSourceType {
			/// The composite function provides continuous output.
			Continuous,
			/// The composite function provides discrete output with four unique values per colour cycle.
			DiscreteFourSamplesPerCycle
		};

		/*! Provides information about the type of output the composite sampling function provides, discrete or continuous.

			This is necessary because the CRT implementation samples discretely and therefore can use fewer intermediate
			samples if it can exactly duplicate the sampling rate and placement of the composite sampling function.

			A continuous function is assumed by default.

			@param type The type of output provided by the function supplied to `set_composite_sampling_function`.
			@param offset_of_first_sample The relative position within a full cycle of the colour subcarrier at which the
			first sample falls. E.g. 0.125 means "at 1/8th of the way through the complete cycle".
		*/
		void set_composite_function_type(CompositeSourceType type, float offset_of_first_sample = 0.0f);

		inline void set_visible_area(Rect visible_area) {
		}

		Rect get_rect_for_area(int first_line_after_sync, int number_of_lines, int first_cycle_after_sync, int number_of_cycles, float aspect_ratio);

		inline void set_delegate(Delegate *delegate) {
			delegate_ = delegate;
		}
};

}
}

#endif /* CRT_cpp */
