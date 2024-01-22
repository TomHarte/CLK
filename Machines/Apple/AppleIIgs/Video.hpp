//
//  Video.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 31/10/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#pragma once

#include "../AppleII/VideoSwitches.hpp"
#include "../../../Outputs/CRT/CRT.hpp"
#include "../../../ClockReceiver/ClockReceiver.hpp"

namespace Apple::IIgs::Video {

/*!
	Provides IIgs video output; assumed clocking here is seven times the usual Apple II clock.
	So it'll produce a single line of video every 456 cycles — 65*7 + 1, allowing for the
	stretched cycle.
*/
class Video: public Apple::II::VideoSwitches<Cycles> {
	public:
		Video();
		void set_internal_ram(const uint8_t *);

		bool get_is_vertical_blank(Cycles offset);
		uint8_t get_horizontal_counter(Cycles offset);
		uint8_t get_vertical_counter(Cycles offset);

		void set_new_video(uint8_t);
		uint8_t get_new_video();

		void clear_interrupts(uint8_t);
		uint8_t get_interrupt_register();
		void set_interrupt_register(uint8_t);
		bool get_interrupt_line();

		void notify_clock_tick();

		void set_border_colour(uint8_t);
		void set_text_colour(uint8_t);
		uint8_t get_text_colour();
		uint8_t get_border_colour();

		void set_composite_is_colour(bool);
		bool get_composite_is_colour();

		/// Sets the scan target.
		void set_scan_target(Outputs::Display::ScanTarget *scan_target);

		/// Gets the current scan status.
		Outputs::Display::ScanStatus get_scaled_scan_status() const;

		/// Sets the type of output.
		void set_display_type(Outputs::Display::DisplayType);

		/// Gets the type of output.
		Outputs::Display::DisplayType get_display_type() const;

		/// Determines the period until video might autonomously update its interrupt lines.
		Cycles next_sequence_point() const;

		/// Sets the Mega II interrupt enable state — 1/4-second and VBL interrupts are
		/// generated here.
		void set_megaii_interrupts_enabled(uint8_t);

		uint8_t get_megaii_interrupt_status();

		void clear_megaii_interrupts();

	private:
		Outputs::CRT::CRT crt_;

		// This is coupled to Apple::II::GraphicsMode, but adds detail for the IIgs.
		enum class GraphicsMode {
			Text = 0,
			DoubleText,
			HighRes,
			DoubleHighRes,
			LowRes,
			DoubleLowRes,
			FatLowRes,

			// Additions:
			DoubleHighResMono,
			SuperHighRes
		};
		constexpr bool is_colour_ntsc(GraphicsMode m) { return m >= GraphicsMode::HighRes && m <= GraphicsMode::FatLowRes; }

		GraphicsMode graphics_mode(int row) const {
			if(new_video_ & 0x80) {
				return GraphicsMode::SuperHighRes;
			}

			const auto ii_mode = Apple::II::VideoSwitches<Cycles>::graphics_mode(row);
			switch(ii_mode) {
				// Coupling very much assumed here.
				case Apple::II::GraphicsMode::DoubleHighRes:
					if(new_video_ & 0x20) {
						return GraphicsMode::DoubleHighResMono;
					}
				[[fallthrough]];

				default: return GraphicsMode(int(ii_mode));	break;
			}
		}

		enum class PixelBufferFormat {
			Text, DoubleText, NTSC, NTSCMono, SuperHighRes
		};
		constexpr PixelBufferFormat format_for_mode(GraphicsMode m) {
			switch(m) {
				case GraphicsMode::Text:				return PixelBufferFormat::Text;
				case GraphicsMode::DoubleText:			return PixelBufferFormat::DoubleText;
				default:								return PixelBufferFormat::NTSC;
				case GraphicsMode::DoubleHighResMono:	return PixelBufferFormat::NTSCMono;
				case GraphicsMode::SuperHighRes:		return PixelBufferFormat::SuperHighRes;
			}
		}

		void advance(Cycles);

		uint8_t new_video_ = 0x01;

		class Interrupts {
			public:
				void add(uint8_t value) {
					// Taken literally, status accumulates regardless of being enabled,
					// potentially to be polled, it simply doesn't trigger an interrupt.
					value_ |= value;
					test();
				}

				void clear(uint8_t value) {
					// Zeroes in bits 5 or 6 clear the respective interrupts.
					value_ &= value | ~0x60;
					test();
				}

				void set_control(uint8_t value) {
					// Ones in bits 1 or 2 enable the respective interrupts.
					value_ = (value_ & ~0x6) | (value & 0x6);
					test();
				}

				uint8_t status() const {
					return value_;
				}

				bool active() const {
					return value_ & 0x80;
				}

			private:
				void test() {
					value_ &= 0x7f;
					if((value_ >> 4) & value_ & 0x6) {
						value_ |= 0x80;
					}
				}

				// Overall meaning of value is as per the VGC interrupt register, i.e.
				//
				//	b7: interrupt status;
				//	b6: 1-second interrupt status;
				//	b5: scan-line interrupt status;
				//	b4: reserved;
				//	b3: reserved;
				//	b2: 1-second interrupt enable;
				//	b1: scan-line interrupt enable;
				//	b0: reserved.
				uint8_t value_ = 0x00;
		} interrupts_;

		int cycles_into_frame_ = 0;
		const uint8_t *ram_ = nullptr;

		// The modal colours.
		uint16_t border_colour_ = 0;
		uint8_t border_colour_entry_ = 0;
		uint8_t text_colour_entry_ = 0xf0;
		uint16_t text_colour_ = 0xffff;
		uint16_t background_colour_ = 0;

		// Current pixel output buffer and conceptual format.
		PixelBufferFormat pixels_format_;
		uint16_t *pixels_ = nullptr, *next_pixel_ = nullptr;
		int pixels_start_column_;

		void output_row(int row, int start, int end);

		uint16_t *output_super_high_res(uint16_t *target, int start, int end, int row) const;

		uint16_t *output_text(uint16_t *target, int start, int end, int row) const;
		uint16_t *output_double_text(uint16_t *target, int start, int end, int row) const;
		uint16_t *output_char(uint16_t *target, uint8_t source, int row) const;

		uint16_t *output_low_resolution(uint16_t *target, int start, int end, int row);
		uint16_t *output_fat_low_resolution(uint16_t *target, int start, int end, int row);
		uint16_t *output_double_low_resolution(uint16_t *target, int start, int end, int row);

		uint16_t *output_high_resolution(uint16_t *target, int start, int end, int row);
		uint16_t *output_double_high_resolution(uint16_t *target, int start, int end, int row);
		uint16_t *output_double_high_resolution_mono(uint16_t *target, int start, int end, int row);

		// Super high-res per-line state.
		uint8_t line_control_;
		uint16_t palette_[16];

		// Storage used for fill mode.
		uint16_t *palette_zero_[4] = {nullptr, nullptr, nullptr, nullptr}, palette_throwaway_;

		// Lookup tables and state to assist in the IIgs' mapping from NTSC to RGB.
		//
		// My understanding of the real-life algorithm is: maintain a four-bit buffer.
		// Fill it in a circular fashion. Ordinarily, output the result of looking
		// up the RGB mapping of those four bits of Apple II output (which outputs four
		// bits per NTSC colour cycle), commuted as per current phase. But if the bit
		// being inserted differs from that currently in its position in the shift
		// register, hold the existing output for three shifts.
		//
		// From there I am using the following:

		// Maps from:
		//
		//	b0 = b0 of the shift register
		//	b1 = b4 of the shift register
		//	b2– = current delay count
		//
		// to a new delay count.
		uint8_t ntsc_delay_lookup_[20];
		uint32_t ntsc_shift_ = 0;	// Assumption here: logical shifts will ensue, rather than arithmetic.
		int ntsc_delay_ = 0;

		/// Outputs the lowest 14 bits from @c ntsc_shift_, mapping to RGB.
		/// Phase is derived from @c column.
		uint16_t *output_shift(uint16_t *target, int column);

		// Common getter for the two counters.
		struct Counters {
			Counters(int v, int h) : vertical(v), horizontal(h) {}
			const int vertical, horizontal;
		};
		Counters get_counters(Cycles offset);

		// Marshalls the Mega II-style interrupt state.
		uint8_t megaii_interrupt_mask_ = 0;
		uint8_t megaii_interrupt_state_ = 0;
		int megaii_frame_counter_ = 0;	// To count up to quarter-second interrupts.
};

}
