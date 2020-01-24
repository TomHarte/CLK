//
//  CRT.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 19/07/2015.
//  Copyright 2015 Thomas Harte. All rights reserved.
//

#ifndef CRT_hpp
#define CRT_hpp

#include <array>
#include <cstdint>
#include <limits>
#include <memory>

#include "../ScanTarget.hpp"
#include "Internals/Flywheel.hpp"

namespace Outputs {
namespace CRT {

class CRT;

class Delegate {
	public:
		virtual void crt_did_end_batch_of_frames(CRT *crt, int number_of_frames, int number_of_unexpected_vertical_syncs) = 0;
};

/*!	Models a class 2d analogue output device, accepting a serial stream of data including syncs
	and generating the proper set of output spans. Attempts to act and react exactly as a real
	TV would have to things like irregular or off-spec sync, and includes logic properly to track
	colour phase for colour composite video.
*/
class CRT {
	private:
		// The incoming clock lengths will be multiplied by @c time_multiplier_; this increases
		// precision across the line.
		int time_multiplier_ = 1;

		// Two flywheels regulate scanning; the vertical will have a range much greater than the horizontal;
		// the output divider is what that'll need to be divided by to reduce it into a 16-bit range as
		// posted on to the scan target.
		std::unique_ptr<Flywheel> horizontal_flywheel_, vertical_flywheel_;
		int vertical_flywheel_output_divider_ = 1;
		int cycles_since_horizontal_sync_ = 0;
		Display::ScanTarget::Scan::EndPoint end_point(uint16_t data_offset);

		struct Scan {
			enum Type {
				Sync, Level, Data, Blank, ColourBurst
			} type = Scan::Blank;
			int number_of_cycles = 0, number_of_samples = 0;
			uint8_t phase = 0, amplitude = 0;
		};
		void output_scan(const Scan *scan);

		int16_t colour_burst_angle_ = 0;
		uint8_t colour_burst_amplitude_ = 30;
		int colour_burst_phase_adjustment_ = 0xff;
		bool is_writing_composite_run_ = false;

		int64_t phase_denominator_ = 1;
		int64_t phase_numerator_ = 0;
		int64_t colour_cycle_numerator_ = 1;
		bool is_alernate_line_ = false, phase_alternates_ = false;

		void advance_cycles(int number_of_cycles, bool hsync_requested, bool vsync_requested, const Scan::Type type, int number_of_samples);
		Flywheel::SyncEvent get_next_vertical_sync_event(bool vsync_is_requested, int cycles_to_run_for, int *cycles_advanced);
		Flywheel::SyncEvent get_next_horizontal_sync_event(bool hsync_is_requested, int cycles_to_run_for, int *cycles_advanced);

		Delegate *delegate_ = nullptr;
		int frames_since_last_delegate_call_ = 0;

		bool is_receiving_sync_ = false;			// @c true if the CRT is currently receiving sync (i.e. this is for edge triggering of horizontal sync); @c false otherwise.
		bool is_accumulating_sync_ = false;			// @c true if a sync level has triggered the suspicion that a vertical sync might be in progress; @c false otherwise.
		bool is_refusing_sync_ = false;				// @c true once a vertical sync has been detected, until a prolonged period of non-sync has ended suspicion of an ongoing vertical sync.
		int sync_capacitor_charge_threshold_ = 0;	// Charges up during times of sync and depletes otherwise; needs to hit a required threshold to trigger a vertical sync.
		int cycles_of_sync_ = 0;					// The number of cycles since the potential vertical sync began.
		int cycles_since_sync_ = 0;					// The number of cycles since last in sync, for defeating the possibility of this being a vertical sync.

		int cycles_per_line_ = 1;

		Outputs::Display::ScanTarget *scan_target_ = &Outputs::Display::NullScanTarget::singleton;
		Outputs::Display::ScanTarget::Modals scan_target_modals_;
		static const uint8_t DefaultAmplitude = 80;

#ifndef NDEBUG
		size_t allocated_data_length_ = std::numeric_limits<size_t>::min();
#endif

	public:
		/*!	Constructs the CRT with a specified clock rate, height and colour subcarrier frequency.
			The requested number of buffers, each with the requested number of bytes per pixel,
			is created for the machine to write raw pixel data to.

			@param cycles_per_line The clock rate at which this CRT will be driven, specified as the number
			of cycles expected to take up one whole scanline of the display.

			@param clocks_per_pixel_greatest_common_divisor The GCD of all potential lengths of a pixel
			in terms of the clock rate given as @c cycles_per_line.

			@param height_of_display The number of lines that nominally form one field of the display, rounded
			up to the next whole integer.

			@param colour_cycle_numerator Specifies the numerator for the per-line frequency of the colour subcarrier.

			@param colour_cycle_denominator Specifies the denominator for the per-line frequency of the colour subcarrier.
			The colour subcarrier is taken to have colour_cycle_numerator/colour_cycle_denominator cycles per line.

			@param vertical_sync_half_lines The expected length of vertical synchronisation (equalisation pulses aside),
			in multiples of half a line.

			@param data_type The format that the caller will use for input data.
		*/
		CRT(int cycles_per_line,
			int clocks_per_pixel_greatest_common_divisor,
			int height_of_display,
			Outputs::Display::ColourSpace colour_space,
			int colour_cycle_numerator,
			int colour_cycle_denominator,
			int vertical_sync_half_lines,
			bool should_alternate,
			Outputs::Display::InputDataType data_type);

		/*!	Exactly identical to calling the designated constructor with colour subcarrier information
			looked up by display type.
		*/
		CRT(int cycles_per_line,
			int minimum_cycles_per_pixel,
			Outputs::Display::Type display_type,
			Outputs::Display::InputDataType data_type);

		/*!	Resets the CRT with new timing information. The CRT then continues as though the new timing had
			been provided at construction. */
		void set_new_timing(
			int cycles_per_line,
			int height_of_display,
			Outputs::Display::ColourSpace colour_space,
			int colour_cycle_numerator,
			int colour_cycle_denominator,
			int vertical_sync_half_lines,
			bool should_alternate);

		/*!	Resets the CRT with new timing information derived from a new display type. The CRT then continues
			as though the new timing had been provided at construction. */
		void set_new_display_type(
			int cycles_per_line,
			Outputs::Display::Type display_type);

		/*!	Changes the type of data being supplied as input.
		*/
		void set_new_data_type(Outputs::Display::InputDataType data_type);

		/*!	Sets the CRT's intended aspect ratio — 4.0/3.0 by default.
		*/
		void set_aspect_ratio(float aspect_ratio);

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

		/*!	Declares that the caller has created a run of data via @c begin_data that is at least @c number_of_samples
			long, and that the first @c number_of_samples should be spread over @c number_of_cycles.

			@param number_of_cycles The amount of data to output.

			@param number_of_samples The number of samples of input data to output.

			@see @c begin_data
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

			@param amplitude The amplitude of the colour burst in 1/255ths of the amplitude of the
			positive portion of the wave.
		*/
		void output_colour_burst(int number_of_cycles, uint8_t phase, uint8_t amplitude = DefaultAmplitude);

		/*! Outputs a colour burst exactly in phase with CRT expectations using the idiomatic amplitude.

			@param number_of_cycles The length of the colour burst;
		*/
		void output_default_colour_burst(int number_of_cycles, uint8_t amplitude = DefaultAmplitude);

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
		inline uint8_t *begin_data(std::size_t required_length, std::size_t required_alignment = 1) {
#ifndef NDEBUG
			allocated_data_length_ = required_length;
#endif
			return scan_target_->begin_data(required_length, required_alignment);
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

		/*!	Nominates a section of the display to crop to for output. */
		void set_visible_area(Outputs::Display::Rect visible_area);

		/*!	@returns The rectangle describing a subset of the display, allowing for sync periods. */
		Outputs::Display::Rect get_rect_for_area(
			int first_line_after_sync,
			int number_of_lines,
			int first_cycle_after_sync,
			int number_of_cycles,
			float aspect_ratio) const;

		/*!	Sets the CRT delegate; set to @c nullptr if no delegate is desired. */
		inline void set_delegate(Delegate *delegate) {
			delegate_ = delegate;
		}

		/*! Sets the scan target for CRT output. */
		void set_scan_target(Outputs::Display::ScanTarget *);

		/*!
			Gets current scan status, with time based fields being in the input scale — e.g. if you're supplying
			86 cycles/line and 98 lines/field then it'll return a field duration of 86*98.
		*/
		Outputs::Display::ScanStatus get_scaled_scan_status() const;

		/*! Sets the display type that will be nominated to the scan target. */
		void set_display_type(Outputs::Display::DisplayType);

		/*! Sets the offset to apply to phase when using the PhaseLinkedLuminance8 input data type. */
		void set_phase_linked_luminance_offset(float);

		/*! Sets the input data type. */
		void set_input_data_type(Outputs::Display::InputDataType);

		/*! Sets the output brightness. */
		void set_brightness(float);
};

/*!
	Provides a CRT delegate that will will observe sync mismatches and, when an appropriate threshold is crossed,
	ask its receiver to try a different display frequency.
*/
template <typename Receiver> class CRTFrequencyMismatchWarner: public Outputs::CRT::Delegate {
	public:
		CRTFrequencyMismatchWarner(Receiver &receiver) : receiver_(receiver) {}

		void crt_did_end_batch_of_frames(Outputs::CRT::CRT *crt, int number_of_frames, int number_of_unexpected_vertical_syncs) final {
			frame_records_[frame_record_pointer_ % frame_records_.size()].number_of_frames = number_of_frames;
			frame_records_[frame_record_pointer_ % frame_records_.size()].number_of_unexpected_vertical_syncs = number_of_unexpected_vertical_syncs;
			++frame_record_pointer_;

			if(frame_record_pointer_*2 >= frame_records_.size()*3) {
				int total_number_of_frames = 0;
				int total_number_of_unexpected_vertical_syncs = 0;
				for(const auto &record: frame_records_) {
					total_number_of_frames += record.number_of_frames;
					total_number_of_unexpected_vertical_syncs += record.number_of_unexpected_vertical_syncs;
				}

				if(total_number_of_unexpected_vertical_syncs >= total_number_of_frames >> 1) {
					reset();
					receiver_.register_crt_frequency_mismatch();
				}
			}
		}

		void reset() {
			for(auto &record: frame_records_) {
				record.number_of_frames = 0;
				record.number_of_unexpected_vertical_syncs = 0;
			}
		}

	private:
		Receiver &receiver_;
		struct FrameRecord {
			int number_of_frames = 0;
			int number_of_unexpected_vertical_syncs = 0;
		};
		std::array<FrameRecord, 4> frame_records_;
		size_t frame_record_pointer_ = 0;
};

}
}

#endif /* CRT_cpp */
