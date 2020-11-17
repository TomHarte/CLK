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
#if TARGET_RT_BIG_ENDIAN
#define PaletteConvulve(x)	uint16_t(x)
#else
#define PaletteConvulve(x)	uint16_t(((x&0xf00) >> 8) | ((x&0x0ff) << 8))
#endif

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
	crt_.set_visible_area(Outputs::Display::Rect(0.097f, 0.1f, 0.85f, 0.85f));
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

Cycles VideoBase::get_next_sequence_point() const {
	const int cycles_into_row = cycles_into_frame_ % CyclesPerLine;
	const int row = cycles_into_frame_ / CyclesPerLine;

	constexpr int sequence_point_offset = (5 + 8) * CyclesPerTick;

	// Handle every case that doesn't involve wrapping to the next row 0.
	if(row <= 200) {
		if(cycles_into_row < sequence_point_offset) return Cycles(sequence_point_offset - cycles_into_row);
		if(row < 200) return Cycles(CyclesPerLine + sequence_point_offset - cycles_into_row);
	}

	// Calculate distance to the relevant point in row 0.
	return Cycles(CyclesPerLine + sequence_point_offset - cycles_into_row + (Lines - row - 1)*CyclesPerLine);
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
	//	5 cycles of back porch;	[TODO: include a colour burst]
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

	// Pixel or pure border => blank as usual.

	// Output blank only at the end of its window.
	if(start < blank_ticks && end >= blank_ticks) {
		crt_.output_blank(blank_ticks * CyclesPerTick);
		start = blank_ticks;
		if(start == end) return;
	}

	// Possibly output border, pixels, border, if this is a pixel line.
	if(row < 192 + ((new_video_&0x80) >> 4)) {	// i.e. 192 lines for classic Apple II video, 200 for IIgs video.

		// Output left border as far as currently known.
		if(start >= start_of_left_border && start < start_of_pixels) {
			const int end_of_period = std::min(start_of_pixels, end);

			uint16_t *const pixel = reinterpret_cast<uint16_t *>(crt_.begin_data(2, 2));
			if(pixel) *pixel = border_colour_;
			crt_.output_data((end_of_period - start) * CyclesPerTick, 1);

			start = end_of_period;
			if(start == end) return;
		}

		// Fetch and output such pixels as it is time for.
		if(start >= start_of_pixels && start < start_of_right_border) {
			const int end_of_period = std::min(start_of_right_border, end);

			if(start == start_of_pixels) {
				// 640 is the absolute most number of pixels that might be generated
				next_pixel_ = pixels_ = reinterpret_cast<uint16_t *>(crt_.begin_data(640, 2));

				// YUCKY HACK. I do not know when the IIgs fetches its super high-res palette
				// and control byte. Since I do not know, any guess is equally likely negatively
				// to affect software. Therefore this hack is as good as any other guess:
				// assume RAM has magical burst bandwidth, and fetch the whole set instantly.
				// I could spread this stuff out to allow for real bandwidth, but it'd likely be
				// no more accurate, while having less of an obvious I-HACKED-THIS red flag attached.
				line_control_ = ram_[0x19d00 + row];
				const int palette_base = (line_control_ & 15) * 16 + 0x19e00;
				for(int c = 0; c < 16; c++) {
					const int entry = ram_[palette_base + (c << 1)] | (ram_[palette_base + (c << 1) + 1] << 8);
					palette_[c] = PaletteConvulve(entry);
				}

				// Post an interrupt if requested.
				if(line_control_ & 0x40) {
					set_interrupts(0x20);
				}
			}

			if(next_pixel_) {
				const int window_start = start - start_of_pixels;
				const int window_end = end_of_period - start_of_pixels;

				if(new_video_ & 0x80) {
					next_pixel_ = output_super_high_res(next_pixel_, window_start, window_end, row);
				} else {
					next_pixel_ = output_text(next_pixel_, window_start, window_end, row);

					// TODO: support modes other than 40-column text.
					if(graphics_mode(row) != Apple::II::GraphicsMode::Text) printf("Outputting incorrect graphics mode!\n");
				}
			}

			if(end_of_period == start_of_right_border) {
				crt_.output_data((start_of_right_border - start_of_pixels) * CyclesPerTick, next_pixel_ ? size_t(next_pixel_ - pixels_) : 1);
				next_pixel_ = pixels_ = nullptr;
			}

			start = end_of_period;
			if(start == end) return;
		}

		// Output right border as far as currently known.
		if(start >= start_of_right_border && start < start_of_sync) {
			const int end_of_period = std::min(start_of_sync, end);

			uint16_t *const pixel = reinterpret_cast<uint16_t *>(crt_.begin_data(2, 2));
			if(pixel) *pixel = border_colour_;
			crt_.output_data((end_of_period - start) * CyclesPerTick, 1);

			// There's no point updating start here; just fall
			// through to the end == FinalColumn test.
		}
	} else {
		// This line is all border, all the time.
		if(start >= start_of_left_border && start < start_of_sync) {
			const int end_of_period = std::min(start_of_sync, end);

			uint16_t *const pixel = reinterpret_cast<uint16_t *>(crt_.begin_data(2, 2));
			if(pixel) *pixel = border_colour_;
			crt_.output_data((end_of_period - start) * CyclesPerTick, 1);

			start = end_of_period;
			if(start == end) return;
		}
	}

	// Output sync if the moment has arrived.
	if(end == FinalColumn) {
		crt_.output_sync(sync_period);
	}
}

bool VideoBase::get_is_vertical_blank() {
	// Cf. http://www.1000bit.it/support/manuali/apple/technotes/iigs/tn.iigs.040.html ;
	// this bit covers the entire vertical border area, not just the NTSC-sense vertical blank.
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
	border_colour_ = appleii_palette[colour & 0xf];
}

void VideoBase::set_text_colour(uint8_t colour) {
	text_colour_ = appleii_palette[colour >> 4];
	background_colour_ = appleii_palette[colour & 0xf];
}

void VideoBase::set_composite_is_colour(bool) {
}

bool VideoBase::get_composite_is_colour() {
	return true;
}

// MARK: - Outputters.

uint16_t *VideoBase::output_text(uint16_t *target, int start, int end, int row) const {
	const uint16_t row_address = get_row_address(row);
	for(int c = start; c < end; c++) {
		const uint8_t source = ram_[row_address + c];
		const int character = source & character_zones_[source >> 6].address_mask;
		const uint8_t xor_mask = character_zones_[source >> 6].xor_mask;
		const std::size_t character_address = size_t(character << 3) + (row & 7);
		const uint8_t character_pattern = character_rom_[character_address] ^ xor_mask;
		const uint16_t colours[2] = {background_colour_, text_colour_};

		target[0] = colours[(character_pattern & 0x40) >> 6];
		target[1] = colours[(character_pattern & 0x20) >> 5];
		target[2] = colours[(character_pattern & 0x10) >> 4];
		target[3] = colours[(character_pattern & 0x08) >> 3];
		target[4] = colours[(character_pattern & 0x04) >> 2];
		target[5] = colours[(character_pattern & 0x02) >> 1];
		target[6] = colours[(character_pattern & 0x01) >> 0];
		target += 7;
	}

	return target;
}

uint16_t *VideoBase::output_super_high_res(uint16_t *target, int start, int end, int row) const {
	// TODO: both the palette and the mode byte should have been fetched by now, and just be
	// available. I haven't implemented that yet, so the below just tries to show _something_.
	// The use of appleii_palette is complete nonsense, as is the assumption of two pixels per byte.

	const int row_address = row * 160 + 0x12000;

	// TODO: line_control_ & 0x20 should enable or disable colour fill mode.
	if(line_control_ & 0x80) {
		for(int c = start * 4; c < end * 4; c++) {
			const uint8_t source = ram_[row_address + c];
			target[0] =  palette_[(source >> 6) & 0x3 + 0x8];
			target[1] =  palette_[(source >> 4) & 0x3 + 0xc];
			target[2] =  palette_[(source >> 2) & 0x3 + 0x0];
			target[3] =  palette_[(source >> 0) & 0x3 + 0x4];
			target += 4;
		}
	} else {
		for(int c = start * 4; c < end * 4; c++) {
			const uint8_t source = ram_[row_address + c];
			target[0] =  palette_[(source >> 4) & 0xf];
			target[1] =  palette_[source & 0xf];
			target += 2;
		}
	}

	return target;
}
