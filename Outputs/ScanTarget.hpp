//
//  ScanTarget.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 30/10/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#ifndef Outputs_Display_ScanTarget_h
#define Outputs_Display_ScanTarget_h

#include <cstddef>
#include <cstdint>
#include "../ClockReceiver/TimeTypes.hpp"

namespace Outputs {
namespace Display {

enum class Type {
	PAL50,
	PAL60,
	NTSC60
};

struct Rect {
	struct Point {
		float x, y;
	} origin;

	struct {
		float width, height;
	} size;

	constexpr Rect() : origin({0.0f, 0.0f}), size({1.0f, 1.0f}) {}
	constexpr Rect(float x, float y, float width, float height) :
		origin({x, y}), size({width, height}) {}
};

enum class ColourSpace {
	/// YIQ is the NTSC colour space.
	YIQ,

	/// YUV is the PAL colour space.
	YUV
};

enum class DisplayType {
	RGB,
	SVideo,
	CompositeColour,
	CompositeMonochrome
};

/*!
	Enumerates the potential formats of input data.
*/
enum class InputDataType {

	// The luminance types can be used to feed only two video pipelines:
	// black and white video, or composite colour.

	Luminance1,				// 1 byte/pixel; any bit set => white; no bits set => black.
	Luminance8,				// 1 byte/pixel; linear scale.

	PhaseLinkedLuminance8,	// 4 bytes/pixel; each byte is an individual 8-bit luminance
							// value and which value is output is a function of
							// colour subcarrier phase — byte 0 defines the first quarter
							// of each colour cycle, byte 1 the next quarter, etc. This
							// format is intended to permit replay of sampled original data.

	// The luminance plus phase types describe a luminance and the phase offset
	// of a colour subcarrier. So they can be used to generate a luminance signal,
	// or an s-video pipeline.

	Luminance8Phase8,		// 2 bytes/pixel; first is luminance, second is phase.
							// Phase is encoded on a 192-unit circle; anything
							// greater than 192 implies that the colour part of
							// the signal should be omitted.

	// The RGB types can directly feed an RGB pipeline, naturally, or can be mapped
	// to phase+luminance, or just to luminance.

	Red1Green1Blue1,		// 1 byte/pixel; bit 0 is blue on or off, bit 1 is green, bit 2 is red.
	Red2Green2Blue2,		// 1 byte/pixel; bits 0 and 1 are blue, bits 2 and 3 are green, bits 4 and 5 are blue.
	Red4Green4Blue4,		// 2 bytes/pixel; first nibble is red, second is green, third is blue.
	Red8Green8Blue8,		// 4 bytes/pixel; first is red, second is green, third is blue, fourth is vacant.
};

inline size_t size_for_data_type(InputDataType data_type) {
	switch(data_type) {
		case InputDataType::Luminance1:
		case InputDataType::Luminance8:
		case InputDataType::Red1Green1Blue1:
		case InputDataType::Red2Green2Blue2:
			return 1;

		case InputDataType::Luminance8Phase8:
		case InputDataType::Red4Green4Blue4:
			return 2;

		case InputDataType::Red8Green8Blue8:
		case InputDataType::PhaseLinkedLuminance8:
			return 4;

		default:
			return 0;
	}
}

inline DisplayType natural_display_type_for_data_type(InputDataType data_type) {
	switch(data_type) {
		default:
		case InputDataType::Luminance1:
		case InputDataType::Luminance8:
		case InputDataType::PhaseLinkedLuminance8:
			return DisplayType::CompositeColour;

		case InputDataType::Red1Green1Blue1:
		case InputDataType::Red2Green2Blue2:
		case InputDataType::Red4Green4Blue4:
		case InputDataType::Red8Green8Blue8:
			return DisplayType::RGB;

		case InputDataType::Luminance8Phase8:
			return DisplayType::SVideo;
	}
}

/*!
	Provides an abstract target for 'scans' i.e. continuous sweeps of output data,
	which are identified by 2d start and end coordinates, and the PCM-sampled data
	that is output during the sweep.

	Additional information is provided to allow decoding (and/or encoding) of a
	composite colour feed.

	Otherwise helpful: the ScanTarget vends all allocated memory. That should allow
	for use of shared memory where available.
*/
struct ScanTarget {
		virtual ~ScanTarget() {}


	/*
		This top section of the interface deals with modal settings. A ScanTarget can
		assume that the modals change very infrequently.
	*/

		struct Modals {
			/// Describes the format of input data.
			InputDataType input_data_type;

			struct InputDataTweaks {
				/// If using the PhaseLinkedLuminance8 data type, this value provides an offset
				/// to add to phase before indexing the supplied luminances.
				float phase_linked_luminance_offset = 0.0f;

			} input_data_tweaks;

			/// Describes the type of display that the data is being shown on.
			DisplayType display_type = DisplayType::SVideo;

			/// If being fed composite data, this defines the colour space in use.
			ColourSpace composite_colour_space;

			/// Provides an integral clock rate for the duration of "a single line", specifically
			/// for an idealised line. So e.g. in NTSC this will be for the duration of 227.5
			/// colour clocks, regardless of whether the source actually stretches lines to
			/// 228 colour cycles, abbreviates them to 227 colour cycles, etc.
			int cycles_per_line;

			/// Sets a GCD for the durations of pixels coming out of this device. This with
			/// the @c cycles_per_line are offered for sizing of intermediary buffers.
			int clocks_per_pixel_greatest_common_divisor;

			/// Provides the number of colour cycles in a line, as a quotient.
			int colour_cycle_numerator, colour_cycle_denominator;

			/// Provides a pre-estimate of the likely number of left-to-right scans per frame.
			/// This isn't a guarantee, but it should provide a decent-enough estimate.
			int expected_vertical_lines;

			/// Provides an additional restriction on the section of the display that is expected
			/// to contain interesting content.
			Rect visible_area;

			/// Describes the usual gamma of the output device these scans would appear on.
			float intended_gamma = 2.2f;

			/// Provides a brightness multiplier for the display output.
			float brightness = 1.0f;

			/// Specifies the range of values that will be output for x and y coordinates.
			struct {
				uint16_t x, y;
			} output_scale;

			/// Describes the intended display aspect ratio.
			float aspect_ratio = 4.0f / 3.0f;
		};

		/// Sets the total format of input data.
		virtual void set_modals(Modals) = 0;


	/*
		This second section of the interface allows provision of the streamed data, plus some control
		over the streaming.
	*/

		/*!
			Defines a scan in terms of its two endpoints.
		*/
		struct Scan {
			struct EndPoint {
				/// Provide the coordinate of this endpoint. These are fixed point, purely fractional
				/// numbers, relative to the scale provided in the Modals.
				uint16_t x, y;

				/// Provides the offset, in samples, into the most recently allocated write area, of data
				/// at this end point.
				uint16_t data_offset;

				/// For composite video, provides the angle of the colour subcarrier at this endpoint.
				///
				/// This is a slightly weird fixed point, being:
				///
				///		* a six-bit fractional part;
				///		* a nine-bit integral part; and
				///		* a sign.
				///
				/// Positive numbers indicate that the colour subcarrier is 'running positively' on this
				/// line; i.e. it is any NTSC line or an appropriate swing PAL line, encoded as
				/// x*cos(a) + y*sin(a).
				///
				/// Negative numbers indicate a 'negative running' colour subcarrier; i.e. it is one of
				/// the phase alternated lines of PAL, encoded as x*cos(a) - y*sin(a), or x*cos(-a) + y*sin(-a),
				/// whichever you prefer.
				///
				/// It will produce undefined behaviour if signs differ on a single scan.
				int16_t composite_angle;

				/// Gives the number of cycles since the most recent horizontal retrace ended.
				uint16_t cycles_since_end_of_horizontal_retrace;
			} end_points[2];

			/// For composite video, dictates the amplitude of the colour subcarrier as a proportion of
			/// the whole, as determined from the colour burst. Will be 0 if there was no colour burst.
			uint8_t composite_amplitude;
		};

		/// Requests a new scan to populate.
		///
		/// @return A valid pointer, or @c nullptr if insufficient further storage is available.
		virtual Scan *begin_scan() = 0;

		/// Requests a new scan to populate.
		virtual void end_scan() {}

		/// Finds the first available storage of at least @c required_length pixels in size which is
		/// suitably aligned for writing of @c required_alignment number of samples at a time.
		///
		/// Calls will be paired off with calls to @c end_data.
		///
		/// @returns a pointer to the allocated space if any was available; @c nullptr otherwise.
		virtual uint8_t *begin_data(size_t required_length, size_t required_alignment = 1) = 0;

		/// Announces that the owner is finished with the region created by the most recent @c begin_data
		/// and indicates that its actual final size was @c actual_length.
		///
		/// It is required that every call to begin_data be paired with a call to end_data.
		virtual void end_data([[maybe_unused]] size_t actual_length) {}

		/// Tells the scan target that its owner is about to change; this is a hint that existing
		/// data and scan allocations should be invalidated.
		virtual void will_change_owner() {}

		/// Acts as a fence, marking the end of an atomic set of [begin/end]_[scan/data] calls] — all future pieces of
		/// data will have no relation to scans prior to the submit() and all future scans will similarly have no relation to
		/// prior runs of data.
		///
		/// Drawing is defined to be best effort, so the scan target should either:
		///
		///		(i)	output everything received since the previous submit; or
		///		(ii)	output nothing.
		///
		/// If there were any allocation failures — i.e. any nullptr responses to begin_data or
		/// begin_scan — then (ii) is a required response. But a scan target may also need to opt for (ii)
		/// for any other reason.
		///
		/// The ScanTarget isn't bound to take any drawing action immediately; it may sit on submitted data for
		/// as long as it feels is appropriate, subject to a @c flush.
		virtual void submit() {}


	/*
		ScanTargets also receive notification of certain events that may be helpful in processing, particularly
		for synchronising internal output to the outside world.
	*/

		enum class Event {
			BeginHorizontalRetrace,
			EndHorizontalRetrace,

			BeginVerticalRetrace,
			EndVerticalRetrace,
		};

		/*!
			Provides a hint that the named event has occurred.

			Guarantee:
			* any announce acts as an implicit fence on data/scans, much as a submit().

			Permitted ScanTarget implementation:
			* ignore all output during retrace periods.

			@param event The event.
			@param is_visible @c true if the output stream is visible immediately after this event; @c false otherwise.
			@param location The location of the event.
			@param composite_amplitude The amplitude of the colour burst on this line (0, if no colour burst was found).
		*/
		virtual void announce([[maybe_unused]] Event event, [[maybe_unused]] bool is_visible, [[maybe_unused]] const Scan::EndPoint &location, [[maybe_unused]] uint8_t composite_amplitude) {}
};

struct ScanStatus {
	/// The current (prediced) length of a field (including retrace).
	Time::Seconds field_duration;
	/// The difference applied to the field_duration estimate during the last field.
	Time::Seconds field_duration_gradient;
	/// The amount of time this device spends in retrace.
	Time::Seconds retrace_duration;
	/// The distance into the current field, from a small negative amount (in retrace) through
	/// 0 (start of visible area field) to 1 (end of field).
	///
	/// This will increase monotonically, being a measure
	/// of the current vertical position — i.e. if current_position = 0.8 then a caller can
	/// conclude that the top 80% of the visible part of the display has been painted.
	float current_position;
	/// The total number of hsyncs so far encountered;
	int hsync_count;
	/// @c true if retrace is currently going on; @c false otherwise.
	bool is_in_retrace;

	/*!
		@returns this ScanStatus, with time-relative fields scaled by dividing them by @c dividend.
	*/
	ScanStatus operator / (float dividend) {
		const ScanStatus result = {
			.field_duration = field_duration / dividend,
			.field_duration_gradient = field_duration_gradient / dividend,
			.retrace_duration = retrace_duration / dividend,
			.current_position = current_position,
			.hsync_count = hsync_count,
			.is_in_retrace = is_in_retrace,
		};
		return result;
	}

	/*!
		@returns this ScanStatus, with time-relative fields scaled by multiplying them by @c multiplier.
	*/
	ScanStatus operator * (float multiplier) {
		const ScanStatus result = {
			.field_duration = field_duration * multiplier,
			.field_duration_gradient = field_duration_gradient * multiplier,
			.retrace_duration = retrace_duration * multiplier,
			.current_position = current_position,
			.hsync_count = hsync_count,
			.is_in_retrace = is_in_retrace,
		};
		return result;
	}
};

/*!
	Provides a null target for scans.
*/
struct NullScanTarget: public ScanTarget {
	void set_modals(Modals) override {}
	Scan *begin_scan() override { return nullptr; }
	uint8_t *begin_data(size_t, size_t) override { return nullptr; }
	void submit() override {}

	static NullScanTarget singleton;
};

}
}

#endif /* Outputs_Display_ScanTarget_h */
