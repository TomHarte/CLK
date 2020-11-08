//
//  Video.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 31/10/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#include "Video.hpp"

using namespace Apple::IIgs::Video;

namespace {

constexpr int CyclesPerTick = 7;	// One 'tick' being the non-stretched length of a cycle on the old Apple II 1Mhz clock.
constexpr int CyclesPerLine = 456;	// Each of the Mega II's cycles lasts 7 cycles, making 455/line except for the
									// final on on a line which lasts an additional 1 (i.e. is 1/7th longer).
constexpr int Lines = 263;
constexpr int FinalPixelLine = 192;

constexpr auto FinalColumn = CyclesPerLine / CyclesPerTick;

// Converts from Apple's RGB ordering to this emulator's.
#define PaletteConvulve(x)	((x&0xf00) >> 8) | ((x&0x00f) << 8) | (x&0x0f0)

// The 12-bit values used by the Apple IIgs to approximate Apple II colours,
// as implied by tech note #63's use of them as border colours.
// http://www.1000bit.it/support/manuali/apple/technotes/iigs/tn.iigs.063.html
constexpr uint16_t appleii_palette[16] = {
	PaletteConvulve(0x0000),	// Black.
	PaletteConvulve(0x0d03),	// Deep Red.
	PaletteConvulve(0x0009),	// Dark Blue.
	PaletteConvulve(0x0d2d),	// Purple.
	PaletteConvulve(0x0072),	// Dark Green.
	PaletteConvulve(0x0555),	// Dark Gray.
	PaletteConvulve(0x022f),	// Medium Blue.
	PaletteConvulve(0x06af),	// Light Blue.
	PaletteConvulve(0x0850),	// Brown.
	PaletteConvulve(0x0f60),	// Orange.
	PaletteConvulve(0x0aaa),	// Light Grey.
	PaletteConvulve(0x0f98),	// Pink.
	PaletteConvulve(0x01d0),	// Light Green.
	PaletteConvulve(0x0ff0),	// Yellow.
	PaletteConvulve(0x04f9),	// Aquamarine.
	PaletteConvulve(0x0fff),	// White.
};

}

VideoBase::VideoBase() :
	VideoSwitches<Cycles>(true, Cycles(2), [this] (Cycles cycles) { advance(cycles); }),
	crt_(CyclesPerLine - 1, 1, Outputs::Display::Type::NTSC60, Outputs::Display::InputDataType::Red4Green4Blue4) {
	crt_.set_display_type(Outputs::Display::DisplayType::RGB);
}

void VideoBase::set_scan_target(Outputs::Display::ScanTarget *scan_target) {
	crt_.set_scan_target(scan_target);
}

Outputs::Display::ScanStatus VideoBase::get_scaled_scan_status() const {
	return crt_.get_scaled_scan_status();
}

void VideoBase::set_display_type(Outputs::Display::DisplayType display_type) {
	crt_.set_display_type(display_type);
}

Outputs::Display::DisplayType VideoBase::get_display_type() const {
	return crt_.get_display_type();
}

void VideoBase::set_internal_ram(const uint8_t *ram) {
	ram_ = ram;
}

void VideoBase::advance(Cycles cycles) {
	const int column_start = (cycles_into_frame_ % CyclesPerLine) / CyclesPerTick;
	const int row_start = cycles_into_frame_ / CyclesPerLine;

	cycles_into_frame_ = (cycles_into_frame_ + cycles.as<int>()) % (CyclesPerLine * Lines);

	const int column_end = (cycles_into_frame_ % CyclesPerLine) / CyclesPerTick;
	const int row_end = cycles_into_frame_ / CyclesPerLine;

	if(row_end == row_start) {
		if(column_end != column_start) {
			output_row(row_start, column_start, column_end);
		}
	} else {
		if(column_start != FinalColumn) {
			output_row(row_start, column_start, FinalColumn);
		}
		for(int row = row_start+1; row < row_end; row++) {
			output_row(row, 0, FinalColumn);
		}
		if(column_end) {
			output_row(row_end, 0, column_end);
		}
	}
}

void VideoBase::output_row(int row, int start, int end) {
	// Reasoned guesswork ahoy!
	//
	// The IIgs VGC can fetch four bytes per column — I'm unclear physically how, but that's definitely true
	// since the IIgs modes packs 160 bytes work of graphics into the Apple II's usual 40-cycle fetch area;
	// it's possible that if I understood the meaning of the linear video bit in the new video flag I'd know more.
	//
	// Super Hi-Res also fetches 16*2 = 32 bytes of palette and a control byte sometime before each row.
	// So it needs five windows for that.
	//
	// Guessing four cycles of sync, I've chosen to arrange one output row for this emulator as:
	//
	//	5 cycles of back porch;
	//	8 windows left border, the final five of which fetch palette and control if in IIgs mode;
	//	40 windows of pixel output;
	//	8 cycles of right border;
	//	4 cycles of sync (including the extra 1/7th window, as it has to go _somewhere_).
	//
	// Otherwise, the first 200 rows may be pixels and the 192 in the middle of those are the II set.
	constexpr int first_sync_line = 220;	// A complete guess. Information needed.

	constexpr int blank_ticks = 5;
	constexpr int left_border_ticks = 8;
	constexpr int pixel_ticks = 40;
	constexpr int right_border_ticks = 8;

	constexpr int start_of_left_border = blank_ticks;
	constexpr int start_of_pixels = start_of_left_border + left_border_ticks;
	constexpr int start_of_right_border = start_of_pixels + pixel_ticks;
	constexpr int start_of_sync = start_of_right_border + right_border_ticks;
	constexpr int sync_period = CyclesPerLine - start_of_sync*CyclesPerTick;

	// Deal with vertical sync.
	if(row >= first_sync_line && row < first_sync_line + 3) {
		// Simplification: just output the whole line at line's end.
		if(end == FinalColumn) {
			crt_.output_sync(CyclesPerLine - sync_period);
			crt_.output_blank(sync_period);
		}

		return;
	}

	// Output pixels.
	if(row < 900) {	// TODO: use real test here; should be 192 for classic Apple II modes.

		// Output blank only at the end of its window.
		if(start < blank_ticks && end >= blank_ticks) {
			crt_.output_blank(blank_ticks * CyclesPerTick);
			start = blank_ticks;
			if(start == end) return;
		}

		// Output left border as far as currently known.
		if(start >= start_of_left_border && start < start_of_pixels) {
			const int end_of_period = std::min(start_of_pixels, end);

			uint16_t *const pixel = reinterpret_cast<uint16_t *>(crt_.begin_data(2, 2));
			if(pixel) *pixel = border_colour_;
			crt_.output_data((end_of_period - start) * CyclesPerTick, 1);

			start = end_of_period;
			if(start == end) return;
		}

		// Output left border as far as currently known.
		if(start >= start_of_pixels && start < start_of_right_border) {
			const int end_of_period = std::min(start_of_right_border, end);

			// TODO: output real pixels.
			uint16_t *const pixel = reinterpret_cast<uint16_t *>(crt_.begin_data(2, 2));
			if(pixel) *pixel = 0xfff;//appleii_palette[15];
			crt_.output_data((end_of_period - start) * CyclesPerTick, 1);

			start = end_of_period;
			if(start == end) return;
		}

		// Output left border as far as currently known.
		if(start >= start_of_right_border && start < start_of_sync) {
			const int end_of_period = std::min(start_of_sync, end);

			uint16_t *const pixel = reinterpret_cast<uint16_t *>(crt_.begin_data(2, 2));
			if(pixel) *pixel = border_colour_;
			crt_.output_data((end_of_period - start) * CyclesPerTick, 1);

			// There's no point updating start here; just fall
			// through to the end == FinalColumn test.
		}

		// Output sync if the moment has arrived.
		if(end == FinalColumn) {
			crt_.output_sync(sync_period);
		}

		return;
	}

	// Not a vertical sync or pixel line => pure border.

	assert(false);
}

bool VideoBase::get_is_vertical_blank() {
	return cycles_into_frame_ >= FinalPixelLine * CyclesPerLine;
}

void VideoBase::set_new_video(uint8_t new_video) {
	new_video_ = new_video;
}

uint8_t VideoBase::get_new_video() {
	return new_video_;
}

void VideoBase::clear_interrupts(uint8_t mask) {
	set_interrupts(interrupts_ & ~(mask & 0x60));
}

void VideoBase::set_interrupt_register(uint8_t mask) {
	set_interrupts(interrupts_ | (mask & 0x6));
}

uint8_t VideoBase::get_interrupt_register() {
	return interrupts_;
}

void VideoBase::notify_clock_tick() {
	set_interrupts(interrupts_ | 0x40);
}

void VideoBase::set_interrupts(uint8_t new_value) {
	interrupts_ = new_value & 0x7f;
	if((interrupts_ >> 4) & interrupts_ & 0x6)
		interrupts_ |= 0x80;
}

void VideoBase::set_border_colour(uint8_t colour) {
	border_colour_ = appleii_palette[colour];
}
