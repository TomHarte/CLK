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

namespace Outputs {
namespace Display {

enum class Type {
	PAL50,
	NTSC60
};

enum class VideoSignal {
	RGB,
	SVideo,
	Composite
};

struct Rect {
	struct Point {
		float x, y;
	} origin;

	struct {
		float width, height;
	} size;

	Rect() : origin({0.0f, 0.0f}), size({1.0f, 1.0f}) {}
	Rect(float x, float y, float width, float height) :
		origin({x, y}), size({width, height}) {}
};

enum class ColourSpace {
	/// YIQ is the NTSC colour space.
	YIQ,

	/// YUV is the PAL colour space.
	YUV
};

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

	/*
		This top section of the interface deals with modal settings. A ScanTarget can
		assume that the modals change very infrequently.
	*/

		struct Modals {
			/*!
				Enumerates the potential formats of input data.
			*/
			enum class DataType {

				// The luminance types can be used to feed only two video pipelines:
				// black and white video, or composite colour.

				Luminance1,				// 1 byte/pixel; any bit set => white; no bits set => black.
				Luminance8,				// 1 byte/pixel; linear scale.

				// The luminance plus phase types describe a luminance and the phase offset
				// of a colour subcarrier. So they can be used to generate a luminance signal,
				// or an s-video pipeline.

				Phase8Luminance8,		// 2 bytes/pixel; first is phase, second is luminance.
										// Phase is encoded on a 192-unit circle; anything
										// greater than 192 implies that the colour part of
										// the signal should be omitted.

				// The RGB types can directly feed an RGB pipeline, naturally, or can be mapped
				// to phase+luminance, or just to luminance.

				Red1Green1Blue1,		// 1 byte/pixel; bit 0 is blue on or off, bit 1 is green, bit 2 is red.
				Red2Green2Blue2,		// 1 byte/pixel; bits 0 and 1 are blue, bits 2 and 3 are green, bits 4 and 5 are blue.
				Red4Green4Blue4,		// 2 bytes/pixel; first nibble is red, second is green, third is blue.
				Red8Green8Blue8,		// 4 bytes/pixel; first is red, second is green, third is blue, fourth is vacant.
			} source_data_type;

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

			/// Provides a pre-estimate of the likely number of left-to-right scans per frame.
			/// This isn't a guarantee, but it should provide a decent-enough estimate.
			int expected_vertical_lines;

			/// Provides an additional restriction on the section of the display that is expected
			/// to contain interesting content.
			Rect visible_area;

			/// Describes the usual gamma of the output device these scans would appear on.
			float intended_gamma;

			/// Specifies the range of values that will be output for x and y coordinates.
			struct {
				uint16_t x, y;
			} output_scale;
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
			} end_points[2];

			/// For composite video, dictates the amplitude of the colour subcarrier as a proportion of
			/// the whole, as determined from the colour burst. Will be 0 if there was no colour burst.
			uint8_t composite_amplitude;
		};

		/// Requests a new scan to populate.
		///
		/// @return A valid pointer, or @c nullptr if insufficient further storage is available.
		virtual Scan *get_scan() = 0;

		/// Finds the first available space of at least @c required_length pixels in size which is suitably aligned
		/// for writing of @c required_alignment number of pixels at a time.
		///
		/// Calls will be paired off with calls to @c reduce_previous_allocation_to.
		///
		/// @returns a pointer to the allocated space if any was available; @c nullptr otherwise.
		virtual uint8_t *allocate_write_area(size_t required_length, size_t required_alignment = 1) = 0;

		/// Announces that the owner is finished with the region created by the most recent @c allocate_write_area
		/// and indicates that its actual final size was @c actual_length.
		virtual void reduce_previous_allocation_to(size_t actual_length) {};

		/// Announces that all endpoint pairs and write areas obtained since the last @c submit have now been
		/// populated with appropriate data.
		///
		/// The ScanTarget isn't bound to take any drawing action immediately; it may sit on submitted data for
		/// as long as it feels is appropriate subject to an @c flush.
		virtual void submit(bool only_if_no_allocation_failures = true) = 0;

		/// Announces that any submitted data not yet output should be output now, but needn't block while
		/// doing so. This generally communicates that processing is now otherwise 'up to date', so no
		/// further delay should be allowed.
//		virtual void flush() = 0;


	/*
		ScanTargets also receive notification of certain events that may be helpful in processing, particularly
		for synchronising internal output to the outside world.
	*/

		enum class Event {
			HorizontalRetrace,
			VerticalRetrace
		};

		/// Provides a hint that the named event has occurred.
		virtual void announce(Event event) {}
};

}
}

#endif /* Outputs_Display_ScanTarget_h */
