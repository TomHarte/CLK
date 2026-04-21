//
//  Video.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/03/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include "Video.hpp"

#include <algorithm>
#include <bit>

using namespace Thomson;

namespace {

constexpr uint16_t rgb(const uint16_t code) {
#ifdef TARGET_RT_BIG_ENDIAN
	return code;
#else
	return std::rotl(code, 8);
#endif
}

// Per https://pulkomandy.tk/wiki/doku.php?id=documentations:devices:gate.arrays#video_generation
constexpr uint16_t mo5_palette[] = {
	rgb(0x000),	rgb(0xf55),	rgb(0x0f0),	rgb(0xff0),	rgb(0x55f),	rgb(0xf0f),	rgb(0x5ff),	rgb(0xfff),
	rgb(0xaaa),	rgb(0xfaa),	rgb(0xafa),	rgb(0xffa),	rgb(0x5af),	rgb(0xfaf),	rgb(0xaff),	rgb(0xfa5),
};

// MARK: - Pixel generators.

// TODO: all of the below ignore the bit ordering. Don't.

template <PixelMode, DotClock, BitOrdering>
struct PixelGenerator {
	static void output(
		uint8_t bank0,
		uint8_t bank1,
		uint16_t *target,
		const std::array<uint16_t, 16> &palette
	);
};

//
// 40-column, and Pages 1 and 2.
//
template <PixelMode mode, DotClock clock, BitOrdering ordering>
void PixelGenerator<mode, clock, ordering>::output(
	const uint8_t bank0,
	const uint8_t bank1,
	uint16_t *const target,
	const std::array<uint16_t, 16> &palette
) {
	const std::array<uint16_t, 2> colours = [&] () -> std::array<uint16_t, 2> {
		switch(mode) {
			case PixelMode::Page1:
				return {
					palette[8],
					palette[9],
				};

			case PixelMode::Page2:
				return {
					palette[8],
					palette[10],
				};

			case PixelMode::Columns40:
				return {
					palette[bank1 & 0xf],
					palette[bank1 >> 4],
				};

			default:
				__builtin_unreachable();
		}
	} ();

	const uint8_t pixels = mode == PixelMode::Page2 ? bank1 : bank0;

	target[0] = colours[(pixels >> 7) & 1];
	target[1] = colours[(pixels >> 6) & 1];
	target[2] = colours[(pixels >> 5) & 1];
	target[3] = colours[(pixels >> 4) & 1];
	if constexpr (pixels_per_column(clock) == 4) {
		return;
	}

	target[4] = colours[(pixels >> 3) & 1];
	target[5] = colours[(pixels >> 2) & 1];
	target[6] = colours[(pixels >> 1) & 1];
	target[7] = colours[(pixels >> 0) & 1];
	if constexpr (pixels_per_column(clock) == 8) {
		return;
	}

	std::fill_n(&target[8], 8, colours[0]);
}


//
// 80-column.
//
template <DotClock clock, BitOrdering ordering>
struct PixelGenerator<PixelMode::Columns80, clock, ordering> {
	static void output(
		const uint8_t bank0,
		const uint8_t bank1,
		uint16_t *const target,
		const std::array<uint16_t, 16> &palette
	) {
		target[0] = palette[(bank0 >> 7) & 1];
		target[1] = palette[(bank1 >> 7) & 1];
		target[2] = palette[(bank0 >> 6) & 1];
		target[3] = palette[(bank1 >> 6) & 1];
		if constexpr (pixels_per_column(clock) == 4) {
			return;
		}

		target[4] = palette[(bank0 >> 5) & 1];
		target[5] = palette[(bank1 >> 5) & 1];
		target[6] = palette[(bank0 >> 4) & 1];
		target[7] = palette[(bank1 >> 4) & 1];
		if constexpr (pixels_per_column(clock) == 8) {
			return;
		}

		target[8] = palette[(bank0 >> 3) & 1];
		target[9] = palette[(bank1 >> 3) & 1];
		target[10] = palette[(bank0 >> 2) & 1];
		target[11] = palette[(bank1 >> 2) & 1];
		target[12] = palette[(bank0 >> 1) & 1];
		target[13] = palette[(bank1 >> 1) & 1];
		target[14] = palette[(bank0 >> 0) & 1];
		target[15] = palette[(bank1 >> 0) & 1];
	}
};

//
// 2bpp.
//
template <DotClock clock, BitOrdering ordering>
struct PixelGenerator<PixelMode::Bitmap4, clock, ordering> {
	static void output(
		const uint8_t bank0,
		const uint8_t bank1,
		uint16_t *const target,
		const std::array<uint16_t, 16> &palette
	) {
		target[0] = palette[(bank0 >> 6) & 0x03];
		target[1] = palette[(bank0 >> 4) & 0x03];
		target[2] = palette[(bank0 >> 2) & 0x03];
		target[3] = palette[(bank0 >> 0) & 0x03];
		if constexpr (pixels_per_column(clock) == 4) {
			return;
		}

		target[4] = palette[(bank1 >> 6) & 0x03];
		target[5] = palette[(bank1 >> 4) & 0x03];
		target[6] = palette[(bank1 >> 2) & 0x03];
		target[7] = palette[(bank1 >> 0) & 0x03];
		if constexpr (pixels_per_column(clock) == 8) {
			return;
		}

		std::fill_n(&target[8], 8, palette[0]);
	}
};

//
// 4bpp.
//
template <DotClock clock, BitOrdering ordering>
struct PixelGenerator<PixelMode::Bitmap16, clock, ordering> {
	static void output(
		const uint8_t bank0,
		const uint8_t bank1,
		uint16_t *const target,
		const std::array<uint16_t, 16> &palette
	) {
		target[0] = palette[(bank0 >> 4) & 0x0f];
		target[1] = palette[(bank0 >> 0) & 0x0f];
		target[2] = palette[(bank1 >> 4) & 0x0f];
		target[3] = palette[(bank1 >> 0) & 0x0f];
		if constexpr (pixels_per_column(clock) == 4) {
			return;
		}

		std::fill_n(&target[4], pixels_per_column(clock) - 4, palette[0]);
	}
};

//
// Overprint.
//
template <DotClock clock, BitOrdering ordering>
struct PixelGenerator<PixelMode::Overprint, clock, ordering> {
	static void output(
		uint8_t bank0,
		uint8_t bank1,
		uint16_t *const target,
		const std::array<uint16_t, 16> &palette
	) {
		const uint16_t colours[] = {
			palette[8],
			palette[10],
			palette[9],
		};

		// Bank 0 appears on top of bank 1.
		bank1 &= ~bank0;

		uint32_t spread = uint32_t((bank0 << 16) | bank1);		// -> 00000000 aaaaaaaa 00000000 bbbbbbbb
		spread = (spread | (spread << 8)) & 0x0f0f0f0f;			// -> 0000aaaa 0000aaaa 0000bbbb 0000bbbb
		spread = (spread | (spread << 4)) & 0x33333333;			// -> 00aa00aa 00aa00aa 00bb00bb 00bb00bb
		spread = (spread | (spread << 2)) & 0x55555555;			// -> 0a0a0a0a 0a0a0a0a 0b0b0b0b 0b0b0b0b

		const uint16_t mix = uint16_t((spread >> 15) | spread);	// -> abababab abababab

		target[0] = colours[(mix >> 14) & 3];
		target[1] = colours[(mix >> 12) & 3];
		target[2] = colours[(mix >> 10) & 3];
		target[3] = colours[(mix >> 8) & 3];
		if constexpr (pixels_per_column(clock) == 4) {
			return;
		}

		target[0] = colours[(mix >> 6) & 3];
		target[1] = colours[(mix >> 4) & 3];
		target[2] = colours[(mix >> 2) & 3];
		target[3] = colours[(mix >> 0) & 3];
		if constexpr (pixels_per_column(clock) == 8) {
			return;
		}

		std::fill_n(&target[8], 8, palette[8]);
	}
};


//
// Triple overprint.
//
template <DotClock clock, BitOrdering ordering>
struct PixelGenerator<PixelMode::TripleOverprint, clock, ordering> {
	static void output(
		uint8_t bank0,
		uint8_t bank1,
		uint16_t *const target,
		const std::array<uint16_t, 16> &palette
	) {
		// Stated rule per https://pulkomandy.tk/wiki/doku.php?id=documentations:devices:gate.arrays is as below:
		//
		//	* Colors 0-7 all become color 0
		//	* Colors 12-15 all become color 12
		//	* Color 11 becomes color 10.
		const uint16_t colours[16] = {
			palette[0],	palette[0],	palette[0],	palette[0],	palette[0],	palette[0],	palette[0],
			palette[8],
			palette[9],
			palette[10],
			palette[10],
			palette[12], palette[12], palette[12], palette[12]
		};

		target[0] = colours[(bank0 >> 4) & 0x0f];
		target[1] = colours[(bank0 >> 0) & 0x0f];
		target[2] = colours[(bank1 >> 4) & 0x0f];
		target[3] = colours[(bank1 >> 0) & 0x0f];
		if constexpr (pixels_per_column(clock) == 4) {
			return;
		}

		std::fill_n(&target[4], pixels_per_column(clock) - 4, palette[0]);
	}
};
}

Video::Video(const uint8_t *const pixels, const uint8_t *const attributes) :
	pixels_(pixels), attributes_(attributes),
	crt_(
		Line::TotalCycles,
		1,
		Outputs::Display::Type::SECAM,
		Outputs::Display::InputDataType::Red4Green4Blue4
	) {
	crt_.set_fixed_framing([&] {
		run_for(Cycles(10'000));
	});
	crt_.set_display_type(Outputs::Display::DisplayType::RGB);

	// Set default palette.
	std::copy(std::begin(mo5_palette), std::end(mo5_palette), std::begin(mapped_palette_));
}

void Video::run_for(const Cycles cycles) {
	position_.advance(
		cycles.as<int>(),
		[&] (const int line, const int begin, const int end) {
			if(line >= Frame::VerticalSyncLine && line < Frame::VerticalSyncLine + Frame::VerticalSyncLength) {
				vsync_line(begin, end);
			} else if(line >= Frame::TotalPixelLines) {
				border_line(begin, end);
			} else {
				#define Opt(x)		case x: pixel_line<x>(begin, end);	break;
				#define Opt2(x)		Opt(x + 0x00);		Opt(x + 0x01);
				#define Opt4(x)		Opt2(x + 0x00);		Opt2(x + 0x02);
				#define Opt8(x)		Opt4(x + 0x00);		Opt4(x + 0x04);
				#define Opt16(x)	Opt8(x + 0x00);		Opt8(x + 0x08);
				#define Opt32(x)	Opt16(x + 0x00);	Opt16(x + 0x10);
				#define Opt64(x)	Opt32(x + 0x00);	Opt32(x + 0x20);
				#define Opt128(x)	Opt64(x + 0x00);	Opt64(x + 0x40);

				switch(mode_ & 0x7f) {
					Opt128(0);
				}

				#undef Opt
				#undef Opt2
				#undef Opt4
				#undef Opt8
				#undef Opt16
				#undef Opt32
				#undef Opt64
				#undef Opt128

			}
		},
		[&] {
			source_address_ = 0;
		}
	);
}

uint8_t Video::vertical_state() const {
	// This is 0xa7e7 in the MO6 documentation, i.e.
	//
	//	b7: 0 = in upper or lower edge; 1 = in pixel zone (instantaneous)
	//	b6: latched version of b7 (but latched by what event?)
	//	b5: 0 = outside window; 1 = inside window (i.e. probably horizontal as well?)

	static constexpr int StartPixelRegion = Line::EndOfLeftBorder;
	static constexpr int EndPixelRegion = (Frame::TotalPixelLines - 1) * Line::TotalCycles + Line::EndOfPixels;

	const bool b7 = position_.absolute() >= StartPixelRegion && position_.absolute() < EndPixelRegion;
	const bool b5 = b7 && position_.subsegment() >= Line::EndOfLeftBorder && position_.subsegment() < Line::EndOfPixels;

	return
		0x01 |		// Leave lower bit for population elsewhere
		(b5 ? 0x20 : 0x00) |
		(b7 ? 0x80 : 0x00);
}

uint8_t Video::horizontal_state() const {
	// 0xa7e4 in MO6 terms, so:
	//
	//	flying spot is:
	//	b7: [LT3] 0 on left-hand edge; 1 on right-hand edge.
	//	b6: [IMIL or possibly /INIL] 0 outside window; 1 inside window.
	return 0x00;
}


void Video::vsync_line(const int, const int line_end) {
	// TODO: resolve coupling here.
	// Supplying sync as it comes to the CRT seems to trigger a fault in retrace start time.
	if(line_end == Line::TotalCycles) {
		crt_.output_sync(Line::TotalCycles); //line_end - line_begin);
	}
}

void Video::border_line(const int line_begin, const int line_end) {
	Numeric::clamp<0, Line::EndOfSync>(line_begin, line_end, [&](const int begin, const int end) {
		crt_.output_sync(end - begin);
	});
	Numeric::clamp<Line::EndOfSync, Line::TotalCycles>(line_begin, line_end, [&](const int begin, const int end) {
		crt_.output_level(end - begin, mapped_border_);
	});
}

template <uint8_t mode>
void Video::pixel_line(const int line_begin, const int line_end) {
	Numeric::clamp<0, Line::EndOfSync>(line_begin, line_end, [&](const int begin, const int end) {
		crt_.output_sync(end - begin);
	});
	Numeric::clamp<Line::EndOfSync, Line::EndOfLeftBorder>(line_begin, line_end, [&](const int begin, const int end) {
		crt_.output_level(end - begin, mapped_border_);
	});
	Numeric::clamp<Line::EndOfLeftBorder, Line::EndOfPixels>(line_begin, line_end, [&](const int begin, const int end) {
		const auto flush_pixels = [&] {
			crt_.output_data(Line::EndOfPixels - output_begin_column_, size_t(output_ - output_begin_));
			output_ = output_begin_ = nullptr;
		};

		// Flush existing pixels if the pixel rate has changed.
		static constexpr auto pixel_rate = pixels_per_column(mode);
		if(output_ && pixel_rate != output_pixel_rate_) {
			flush_pixels();
		}

		// Try to grab a buffer if without one.
		if(!output_) {
			output_ = output_begin_ = reinterpret_cast<uint16_t *>(crt_.begin_data(size_t(pixel_rate * 40)));
			output_begin_column_ = begin;
			output_pixel_rate_ = pixel_rate;
		}

		if(output_) {
			for(int c = begin; c < end; c++) {
				PixelGenerator<pixel_mode(mode), dot_clock(mode), bit_ordering(mode)>::output(
					pixels_[source_address_],
					attributes_[source_address_],
					output_,
					mapped_palette_
				);
				++source_address_;
				output_ += pixel_rate;
			}

			if(end == Line::EndOfPixels) {
				flush_pixels();
			}
		} else {
			source_address_ += end - begin;
			crt_.output_data(end - begin, 1);
		}
	});
	Numeric::clamp<Line::EndOfPixels, Line::TotalCycles>(line_begin, line_end, [&](const int begin, const int end) {
		crt_.output_level(end - begin, mapped_border_);
	});
}

Cycles Video::next_sequence_point() const {
	// Pulse the interrupt output for 8 cycles, arbitrarily. The real number seems to be undocumented, and it
	// doesn't actually make much concrete difference in concrete terms because this is fed into edge-detection via
	// the PIA. Knowing the real number would only fix the case where the detected transition is switched to
	// trailing-edge during the pulse, which probably doesn't happen in any real software.
	//
	// Would be nice to know the real number though.
	if(position_.absolute() < IRQ::Cycle) return IRQ::Cycle - position_.absolute();
	if(position_.absolute() < IRQ::Cycle + IRQ::Length) return IRQ::Cycle + IRQ::Length - position_.absolute();
	return Frame::TotalCycles - position_.absolute() + IRQ::Cycle;
}

bool Video::irq() const {
	return position_.absolute() >= IRQ::Cycle && position_.absolute() < (IRQ::Cycle + IRQ::Length);
}

void Video::set_border_colour(const uint8_t colour) {
	border_ = colour & 0xf;
	update_mapped_border();
}

uint8_t Video::palette_index() const {
	return palette_index_.get();
}

void Video::set_palette_index(const uint8_t index) {
	palette_index_ = index;
}

uint8_t Video::palette() {
	const auto result = palette_[palette_index_.get()];
	++palette_index_;
	return result;
}

void Video::set_palette(const uint8_t value) {
	// Update internal store.
	const auto index = palette_index_.get();
	++palette_index_;
	palette_[index] = value;

	// Update output colour.
	const auto rg = palette_[index & ~1];
	const auto b = palette_[index | 1];
	mapped_palette_[index >> 1] = rgb(uint16_t(
		((rg & 0x0f) << 8) |
		(rg & 0xf0) |
		(b & 0x0f)
	));
	update_mapped_border();
}

void Video::set_mode(const uint8_t value) {
	mode_ = value;
}
