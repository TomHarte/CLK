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


// A table to map from 7-bit integers to 14-bit versions with all bits doubled.
constexpr uint16_t double_bytes[128] = {
	0x0000, 0x0003, 0x000c, 0x000f, 0x0030, 0x0033, 0x003c, 0x003f,
	0x00c0, 0x00c3, 0x00cc, 0x00cf, 0x00f0, 0x00f3, 0x00fc, 0x00ff,
	0x0300, 0x0303, 0x030c, 0x030f, 0x0330, 0x0333, 0x033c, 0x033f,
	0x03c0, 0x03c3, 0x03cc, 0x03cf, 0x03f0, 0x03f3, 0x03fc, 0x03ff,
	0x0c00, 0x0c03, 0x0c0c, 0x0c0f, 0x0c30, 0x0c33, 0x0c3c, 0x0c3f,
	0x0cc0, 0x0cc3, 0x0ccc, 0x0ccf, 0x0cf0, 0x0cf3, 0x0cfc, 0x0cff,
	0x0f00, 0x0f03, 0x0f0c, 0x0f0f, 0x0f30, 0x0f33, 0x0f3c, 0x0f3f,
	0x0fc0, 0x0fc3, 0x0fcc, 0x0fcf, 0x0ff0, 0x0ff3, 0x0ffc, 0x0fff,
	0x3000, 0x3003, 0x300c, 0x300f, 0x3030, 0x3033, 0x303c, 0x303f,
	0x30c0, 0x30c3, 0x30cc, 0x30cf, 0x30f0, 0x30f3, 0x30fc, 0x30ff,
	0x3300, 0x3303, 0x330c, 0x330f, 0x3330, 0x3333, 0x333c, 0x333f,
	0x33c0, 0x33c3, 0x33cc, 0x33cf, 0x33f0, 0x33f3, 0x33fc, 0x33ff,
	0x3c00, 0x3c03, 0x3c0c, 0x3c0f, 0x3c30, 0x3c33, 0x3c3c, 0x3c3f,
	0x3cc0, 0x3cc3, 0x3ccc, 0x3ccf, 0x3cf0, 0x3cf3, 0x3cfc, 0x3cff,
	0x3f00, 0x3f03, 0x3f0c, 0x3f0f, 0x3f30, 0x3f33, 0x3f3c, 0x3f3f,
	0x3fc0, 0x3fc3, 0x3fcc, 0x3fcf, 0x3ff0, 0x3ff3, 0x3ffc, 0x3fff,
};

}

Video::Video() :
	VideoSwitches<Cycles>(true, Cycles(2), [this] (Cycles cycles) { advance(cycles); }),
	crt_(CyclesPerLine - 1, 1, Outputs::Display::Type::NTSC60, Outputs::Display::InputDataType::Red4Green4Blue4) {
	crt_.set_display_type(Outputs::Display::DisplayType::RGB);
	crt_.set_visible_area(Outputs::Display::Rect(0.097f, 0.1f, 0.85f, 0.85f));

	// Establish the shift lookup table for NTSC -> RGB output.
	for(size_t c = 0; c < sizeof(ntsc_delay_lookup_) / sizeof(*ntsc_delay_lookup_); c++) {
		const auto old_delay = c >> 2;

		// If delay is 3, 2, 1 or 0 the output is just that minus 1.
		// Otherwise the output is either still 4, or 3 if the two lowest bits don't match.
		if(old_delay < 4) {
			ntsc_delay_lookup_[c] = (old_delay > 0) ? uint8_t(old_delay - 1) : 4;
		} else {
			ntsc_delay_lookup_[c] = (c&1) == ((c >> 1)&1) ? 4 : 3;
		}

		ntsc_delay_lookup_[c] = 4;
	}
}

void Video::set_scan_target(Outputs::Display::ScanTarget *scan_target) {
	crt_.set_scan_target(scan_target);
}

Outputs::Display::ScanStatus Video::get_scaled_scan_status() const {
	return crt_.get_scaled_scan_status();
}

void Video::set_display_type(Outputs::Display::DisplayType display_type) {
	crt_.set_display_type(display_type);
}

Outputs::Display::DisplayType Video::get_display_type() const {
	return crt_.get_display_type();
}

void Video::set_internal_ram(const uint8_t *ram) {
	ram_ = ram;
}

void Video::advance(Cycles cycles) {
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

Cycles Video::get_next_sequence_point() const {
	const int cycles_into_row = cycles_into_frame_ % CyclesPerLine;
	const int row = cycles_into_frame_ / CyclesPerLine;

	constexpr int sequence_point_offset = (blank_ticks + left_border_ticks) * CyclesPerTick;

	// Handle every case that doesn't involve wrapping to the next row 0.
	if(row <= 200) {
		if(cycles_into_row < sequence_point_offset) return Cycles(sequence_point_offset - cycles_into_row);
		if(row < 200) return Cycles(CyclesPerLine + sequence_point_offset - cycles_into_row);
	}

	// Calculate distance to the relevant point in row 0.
	return Cycles(CyclesPerLine + sequence_point_offset - cycles_into_row + (Lines - row - 1)*CyclesPerLine);
}

void Video::output_row(int row, int start, int end) {

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

	// The pixel buffer will actually be allocated a column early, to allow double high/low res to start
	// half a column before everything else.
	constexpr int pixel_buffer_allocation = start_of_pixels - 1;

	// Possibly output border, pixels, border, if this is a pixel line.
	if(row < 192 + ((new_video_&0x80) >> 4)) {	// i.e. 192 lines for classic Apple II video, 200 for IIgs video.

		// Output left border as far as currently known.
		if(start >= start_of_left_border && start < pixel_buffer_allocation) {
			const int end_of_period = std::min(pixel_buffer_allocation, end);

			if(border_colour_) {
				uint16_t *const pixel = reinterpret_cast<uint16_t *>(crt_.begin_data(2, 2));
				if(pixel) *pixel = border_colour_;
				crt_.output_data((end_of_period - start) * CyclesPerTick, 1);
			} else {
				crt_.output_blank((end_of_period - start) * CyclesPerTick);
			}

			start = end_of_period;
			if(start == end) return;
		}

		assert(end > start);

		// Fetch and output such pixels as it is time for.
		if(start >= pixel_buffer_allocation && start < start_of_right_border) {
			const int end_of_period = std::min(start_of_right_border, end);
			const auto mode = graphics_mode(row);

			if(start == pixel_buffer_allocation) {
				// YUCKY HACK. I do not know when the IIgs fetches its super high-res palette
				// and control byte. Since I do not know, any guess is equally likely negatively
				// to affect software. Therefore this hack is as good as any other guess:
				// assume RAM has magical burst bandwidth, and fetch the whole set instantly.
				// I could spread this stuff out to allow for real bandwidth, but it'd likely be
				// no more accurate, while having less of an obvious I-HACKED-THIS red flag attached.
				line_control_ = ram_[0x19d00 + row];
				const int palette_base = (line_control_ & 15) * 32 + 0x19e00;
				for(int c = 0; c < 16; c++) {
					const int entry = ram_[palette_base + (c << 1)] | (ram_[palette_base + (c << 1) + 1] << 8);
					palette_[c] = PaletteConvulve(entry);
				}

				// Post an interrupt if requested.
				if(line_control_ & 0x40) {
					set_interrupts(0x20);
				}

				// Set up appropriately for fill mode (or not).
				for(int c = 0; c < 4; c++) {
					palette_zero_[c] = (line_control_ & 0x20) ? &palette_[c * 4] : &palette_throwaway_;
				}

				// Reset NTSC decoding and total line buffering.
				ntsc_delay_ = 4;
				pixels_start_column_ = start;
			}

			if(!next_pixel_ || pixels_format_ != format_for_mode(mode)) {
				// Flush anything already in a buffer.
				if(pixels_start_column_ < start) {
					crt_.output_data((start - pixels_start_column_) * CyclesPerTick, next_pixel_ ? size_t(next_pixel_ - pixels_) : 1);
					next_pixel_ = pixels_ = nullptr;
				}

				// Allocate a new buffer; 640 is as bad as it gets.
				// TODO: make proper size estimate?
				next_pixel_ = pixels_ = reinterpret_cast<uint16_t *>(crt_.begin_data(644, 2));
				pixels_start_column_ = start;
				pixels_format_ = format_for_mode(mode);
			}

			if(next_pixel_) {
				int window_start = start - start_of_pixels;
				const int window_end = end_of_period - start_of_pixels;

				// Fill in border colour if this is the first column.
				if(window_start == -1) {
					if(next_pixel_) {
						int extra_border_length;
						switch(mode) {
							case GraphicsMode::DoubleText:
							case GraphicsMode::Text:
							case GraphicsMode::DoubleHighRes:
							case GraphicsMode::DoubleLowRes:
							case GraphicsMode::DoubleHighResMono:
								extra_border_length = 7;
							break;
							case GraphicsMode::HighRes:
							case GraphicsMode::LowRes:
							case GraphicsMode::FatLowRes:
								extra_border_length = 14;
							break;
							case GraphicsMode::SuperHighRes:
								extra_border_length = (line_control_ & 0x80) ? 4 : 2;
							break;
						}
						for(int c = 0; c < extra_border_length; c++) {
							next_pixel_[c] = border_colour_;
						}
						next_pixel_ += extra_border_length;
					}
					++window_start;
					if(window_start == window_end) return;
				}

				switch(mode) {
					case GraphicsMode::SuperHighRes:
						next_pixel_ = output_super_high_res(next_pixel_, window_start, window_end, row);
					break;

					case GraphicsMode::Text:
						next_pixel_ = output_text(next_pixel_, window_start, window_end, row);
					break;
					case GraphicsMode::DoubleText:
						next_pixel_ = output_double_text(next_pixel_, window_start, window_end, row);
					break;

					case GraphicsMode::FatLowRes:
						next_pixel_ = output_fat_low_resolution(next_pixel_, window_start, window_end, row);
					break;
					case GraphicsMode::LowRes:
						next_pixel_ = output_low_resolution(next_pixel_, window_start, window_end, row);
					break;
					case GraphicsMode::DoubleLowRes:
						next_pixel_ = output_double_low_resolution(next_pixel_, window_start, window_end, row);
					break;

					case GraphicsMode::HighRes:
						next_pixel_ = output_high_resolution(next_pixel_, window_start, window_end, row);
					break;
					case GraphicsMode::DoubleHighRes:
						next_pixel_ = output_double_high_resolution(next_pixel_, window_start, window_end, row);
					break;
					case GraphicsMode::DoubleHighResMono:
						next_pixel_ = output_double_high_resolution_mono(next_pixel_, window_start, window_end, row);
					break;

					default:
						assert(false);	// i.e. other modes yet to do.
				}
			}

			if(end_of_period == start_of_right_border) {
				// Flush what remains in the NTSC queue, if applicable.
				// TODO: with real NTSC test, why not?
				if(next_pixel_ && is_colour_ntsc(mode)) {
					ntsc_shift_ >>= 14;
					next_pixel_ = output_shift(next_pixel_, 81);
				}

				crt_.output_data((start_of_right_border - pixels_start_column_) * CyclesPerTick, next_pixel_ ? size_t(next_pixel_ - pixels_) : 1);
				next_pixel_ = pixels_ = nullptr;
			}

			start = end_of_period;
			if(start == end) return;
		}

		assert(end > start);

		// Output right border as far as currently known.
		if(start >= start_of_right_border && start < start_of_sync) {
			const int end_of_period = std::min(start_of_sync, end);

			if(border_colour_) {
				uint16_t *const pixel = reinterpret_cast<uint16_t *>(crt_.begin_data(2, 2));
				if(pixel) *pixel = border_colour_;
				crt_.output_data((end_of_period - start) * CyclesPerTick, 1);
			} else {
				crt_.output_blank((end_of_period - start) * CyclesPerTick);
			}

			// There's no point updating start here; just fall
			// through to the end == FinalColumn test.
		}
	} else {
		// This line is all border, all the time.
		if(start >= start_of_left_border && start < start_of_sync) {
			const int end_of_period = std::min(start_of_sync, end);

			if(border_colour_) {
				uint16_t *const pixel = reinterpret_cast<uint16_t *>(crt_.begin_data(2, 2));
				if(pixel) *pixel = border_colour_;
				crt_.output_data((end_of_period - start) * CyclesPerTick, 1);
			} else {
				crt_.output_blank((end_of_period - start) * CyclesPerTick);
			}

			start = end_of_period;
			if(start == end) return;
		}
	}

	// Output sync if the moment has arrived.
	if(end == FinalColumn) {
		crt_.output_sync(sync_period);
	}
}

bool Video::get_is_vertical_blank(Cycles offset) {
	// Cf. http://www.1000bit.it/support/manuali/apple/technotes/iigs/tn.iigs.040.html ;
	// this bit covers the entire vertical border area, not just the NTSC-sense vertical blank,
	// and considers the border to begin at 192 even though Super High-res mode is 200 lines.
	return (cycles_into_frame_ + offset.as<int>())%(Lines * CyclesPerLine) >= FinalPixelLine * CyclesPerLine;
}

void Video::set_new_video(uint8_t new_video) {
	new_video_ = new_video;
}

uint8_t Video::get_new_video() {
	return new_video_;
}

void Video::clear_interrupts(uint8_t mask) {
	set_interrupts(interrupts_ & ~(mask & 0x60));
}

void Video::set_interrupt_register(uint8_t mask) {
	set_interrupts(interrupts_ | (mask & 0x6));
}

uint8_t Video::get_interrupt_register() {
	return interrupts_;
}

void Video::notify_clock_tick() {
	set_interrupts(interrupts_ | 0x40);
}

void Video::set_interrupts(uint8_t new_value) {
	interrupts_ = new_value & 0x7f;
	if((interrupts_ >> 4) & interrupts_ & 0x6)
		interrupts_ |= 0x80;
}

void Video::set_border_colour(uint8_t colour) {
	border_colour_entry_ = colour & 0x0f;
	border_colour_ = appleii_palette[border_colour_entry_];
}

uint8_t Video::get_border_colour() {
	return border_colour_entry_;
}

void Video::set_text_colour(uint8_t colour) {
	text_colour_ = appleii_palette[colour >> 4];
	background_colour_ = appleii_palette[colour & 0xf];
}

void Video::set_composite_is_colour(bool) {
}

bool Video::get_composite_is_colour() {
	return true;
}

// MARK: - Outputters.

uint16_t *Video::output_char(uint16_t *target, uint8_t source, int row) const {
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
	return target + 7;
}

uint16_t *Video::output_text(uint16_t *target, int start, int end, int row) const {
	const uint16_t row_address = get_row_address(row);
	for(int c = start; c < end; c++) {
		target = output_char(target, ram_[row_address + c], row);
	}

	return target;
}

uint16_t *Video::output_double_text(uint16_t *target, int start, int end, int row) const {
	const uint16_t row_address = get_row_address(row);
	for(int c = start; c < end; c++) {
		target = output_char(target, ram_[0x10000 + row_address + c], row);
		target = output_char(target, ram_[row_address + c], row);
	}

	return target;
}

uint16_t *Video::output_super_high_res(uint16_t *target, int start, int end, int row) const {
	const int row_address = row * 160 + 0x12000;

	// The palette_zero_ writes ensure that palette colour 0 is replaced by whatever was last output,
	// if fill mode is enabled. Otherwise they go to throwaway storage.
	if(line_control_ & 0x80) {
		for(int c = start * 4; c < end * 4; c++) {
			const uint8_t source = ram_[row_address + c];
			*palette_zero_[3] = target[0] = palette_[0x8 + ((source >> 6) & 0x3)];
			*palette_zero_[0] = target[1] = palette_[0xc + ((source >> 4) & 0x3)];
			*palette_zero_[1] = target[2] = palette_[0x0 + ((source >> 2) & 0x3)];
			*palette_zero_[2] = target[3] = palette_[0x4 + ((source >> 0) & 0x3)];
			target += 4;
		}
	} else {
		for(int c = start * 4; c < end * 4; c++) {
			const uint8_t source = ram_[row_address + c];
			*palette_zero_[0] = target[0] = palette_[(source >> 4) & 0xf];
			*palette_zero_[0] = target[1] = palette_[source & 0xf];
			target += 2;
		}
	}

	return target;
}

uint16_t *Video::output_double_high_resolution_mono(uint16_t *target, int start, int end, int row) {
	const uint16_t row_address = get_row_address(row);
	constexpr uint16_t colours[] = {0, 0xffff};
	for(int c = start; c < end; c++) {
		const uint8_t source[2] = {
			ram_[0x10000 + row_address + c],
			ram_[row_address + c],
		};

		target[0] = colours[(source[1] >> 0) & 0x1];
		target[1] = colours[(source[1] >> 1) & 0x1];
		target[2] = colours[(source[1] >> 2) & 0x1];
		target[3] = colours[(source[1] >> 3) & 0x1];
		target[4] = colours[(source[1] >> 4) & 0x1];
		target[5] = colours[(source[1] >> 5) & 0x1];
		target[6] = colours[(source[1] >> 6) & 0x1];

		target[7] = colours[(source[0] >> 0) & 0x1];
		target[8] = colours[(source[0] >> 1) & 0x1];
		target[9] = colours[(source[0] >> 2) & 0x1];
		target[10] = colours[(source[0] >> 3) & 0x1];
		target[11] = colours[(source[0] >> 4) & 0x1];
		target[12] = colours[(source[0] >> 5) & 0x1];
		target[13] = colours[(source[0] >> 6) & 0x1];

		target += 14;
	}

	return target;
}


uint16_t *Video::output_low_resolution(uint16_t *target, int start, int end, int row) {
	const int row_shift = row&4;
	const uint16_t row_address = get_row_address(row);
	for(int c = start; c < end; c++) {
		const uint8_t source = (ram_[row_address + c] >> row_shift) & 0xf;

		// Convulve input as a function of odd/even row.
		uint32_t long_source;
		if(c&1) {
			long_source = uint32_t((source >> 2) | (source << 2) | (source << 6) | (source << 10));
		} else {
			long_source = uint32_t((source | (source << 4) | (source << 8) | (source << 12)) & 0x3fff);
		}

		ntsc_shift_ = (long_source << 18) | (ntsc_shift_ >> 14);
		target = output_shift(target, 1 + c*2);
	}

	return target;
}

uint16_t *Video::output_fat_low_resolution(uint16_t *target, int start, int end, int row) {
	const int row_shift = row&4;
	const uint16_t row_address = get_row_address(row);
	for(int c = start; c < end; c++) {
		const uint32_t doubled_source = uint32_t(double_bytes[(ram_[row_address + c] >> row_shift) & 0xf]);
		const uint32_t long_source = doubled_source | (doubled_source << 8);
		// TODO: verify the above.

		ntsc_shift_ = (long_source << 18) | (ntsc_shift_ >> 14);
		target = output_shift(target, 1 + c*2);
	}

	return target;
}

uint16_t *Video::output_double_low_resolution(uint16_t *target, int start, int end, int row) {
	const int row_shift = row&4;
	const uint16_t row_address = get_row_address(row);
	for(int c = start; c < end; c++) {
		const uint8_t source[2] = {
			uint8_t((ram_[row_address + c] >> row_shift) & 0xf),
			uint8_t((ram_[0x10000 + row_address + c] >> row_shift) & 0xf)
		};

		// Convulve input as a function of odd/even row; this is very much like low
		// resolution mode except that the first 7 bits to be output will come from
		// source[1] and the next 7 from source[0]. Also shifting is offset by
		// half a window compared to regular low resolution, so the conditional
		// works the other way around.
		uint32_t long_source;
		if(c&1) {
			long_source = uint32_t((source[1] | ((source[1] << 4) & 0x70) | ((source[0] << 4) & 0x80) | (source[0] << 8) | (source[0] << 12)) & 0x3fff);
		} else {
			long_source = uint32_t((source[1] >> 2) | (source[1] << 2) | ((source[1] << 6) & 0x40) | ((source[0] << 6) & 0x380) | (source[0] << 10));
		}

		ntsc_shift_ = (long_source << 18) | (ntsc_shift_ >> 14);
		target = output_shift(target, c*2);
	}

	return target;
}

uint16_t *Video::output_high_resolution(uint16_t *target, int start, int end, int row) {
	const uint16_t row_address = get_row_address(row);
	for(int c = start; c < end; c++) {
		uint8_t source = ram_[row_address + c];
		const uint32_t doubled_source = uint32_t(double_bytes[source & 0x7f]);

		// Just append new bits, doubled up (and possibly delayed).
		// TODO: I can kill the conditional here. Probably?
		if(source & high_resolution_mask_ & 0x80) {
			ntsc_shift_ = ((doubled_source & 0x1fff) << 19) | ((ntsc_shift_ >> 13) & 0x40000) | (ntsc_shift_ >> 14);
		} else {
			ntsc_shift_ = (doubled_source << 18) | (ntsc_shift_ >> 14);
		}

		target = output_shift(target, 1 + c*2);
	}

	return target;
}

uint16_t *Video::output_double_high_resolution(uint16_t *target, int start, int end, int row) {
	const uint16_t row_address = get_row_address(row);
	for(int c = start; c < end; c++) {
		const uint8_t source[2] = {
			ram_[0x10000 + row_address + c],
			ram_[row_address + c],
		};

		ntsc_shift_ = unsigned(source[1] << 25) | unsigned(source[0] << 18) | (ntsc_shift_ >> 14);
		target = output_shift(target, c*2);
	}

	return target;
}

uint16_t *Video::output_shift(uint16_t *target, int column) {
	// Make sure that at least two columns are enqueued before output begins;
	// the top bits can't be understood without reference to bits that come afterwards.
	if(!column) {
		ntsc_shift_ |= ntsc_shift_ >> 14;
		return target;
	}

	// Phase here is kind of arbitrary; it pairs off with the order
	// I've picked for my rolls table and with my decision to count
	// columns as aligned with double-mode.
	const int phase = column * 7 + 3;
	constexpr uint8_t rolls[4][16] = {
		{
			0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf
		},
		{
			0x0, 0x2, 0x4, 0x6, 0x8, 0xa, 0xc, 0xe,
			0x1, 0x3, 0x5, 0x7, 0x9, 0xb, 0xd, 0xf
		},
		{
			0x0, 0x4, 0x8, 0xc,		0x1, 0x5, 0x9, 0xd,
			0x2, 0x6, 0xa, 0xe,		0x3, 0x7, 0xb, 0xf
		},
		{
			0x0, 0x8,		0x1, 0x9,		0x2, 0xa,		0x3, 0xb,
			0x4, 0xc,		0x5, 0xd,		0x6, 0xe,		0x7, 0xf
		},
	};

#define OutputPixel(offset)	{\
	ntsc_delay_ = ntsc_delay_lookup_[unsigned(ntsc_delay_ << 2) | ((ntsc_shift_ >> offset)&1) | ((ntsc_shift_ >> (offset + 3))&2)];	\
	const auto raw_bits = (ntsc_shift_ >> (offset + ntsc_delay_)) & 0x0f; \
	target[offset] = appleii_palette[rolls[(phase + offset + ntsc_delay_)&3][raw_bits]];	\
}

	OutputPixel(0);
	OutputPixel(1);
	OutputPixel(2);
	OutputPixel(3);
	OutputPixel(4);
	OutputPixel(5);
	OutputPixel(6);
	OutputPixel(7);
	OutputPixel(8);
	OutputPixel(9);
	OutputPixel(10);
	OutputPixel(11);
	OutputPixel(12);
	OutputPixel(13);

#undef OutputPixel

	return target + 14;
}
