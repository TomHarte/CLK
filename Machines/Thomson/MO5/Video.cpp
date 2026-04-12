//
//  Video.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/03/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include "Video.hpp"

#include <bit>

using namespace Thomson::MO5;

namespace {

constexpr uint16_t rgb(const uint16_t code) {
#ifdef TARGET_RT_BIG_ENDIAN
	return code;
#else
	return std::rotl(code, 8);
#endif
}

// Per https://pulkomandy.tk/wiki/doku.php?id=documentations:devices:gate.arrays#video_generation
constexpr uint16_t palette[] = {
	rgb(0x000),	rgb(0xf55),	rgb(0x0f0),	rgb(0xff0),	rgb(0x55f),	rgb(0xf0f),	rgb(0x5ff),	rgb(0xfff),
	rgb(0xaaa),	rgb(0xfaa),	rgb(0xafa),	rgb(0xffa),	rgb(0x5af),	rgb(0xfaf),	rgb(0xaff),	rgb(0xfa5),
};

}

Video::Video(const uint8_t *const pixels, const uint8_t *const attributes) :
	pixels_(pixels), attributes_(attributes),
	crt_(
		CyclesPerLine,
		1,
		Outputs::Display::Type::SECAM,
		Outputs::Display::InputDataType::Red4Green4Blue4
	) {
	crt_.set_fixed_framing([&] {
		run_for(Cycles(10'000));
	});
	crt_.set_display_type(Outputs::Display::DisplayType::RGB);
}

void Video::run_for(const Cycles cycles) {
	position_.advance(
		cycles.as<int>(),
		[&] (const int line, const int begin, const int end) {
			if(line >= VerticalSyncLine && line < VerticalSyncLine + VerticalSyncLength) {
				vsync_line(begin, end);
			} else if(line >= TotalPixelLines) {
				border_line(begin, end);
			} else {
				pixel_line(begin, end);
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

	static constexpr int StartPixelRegion = EndOfLeftBorder;
	static constexpr int EndPixelRegion = (TotalPixelLines - 1) * CyclesPerLine + EndOfPixels;

	const bool b7 = position_.absolute() >= StartPixelRegion && position_.absolute() < EndPixelRegion;
	const bool b5 = b7 && position_.subsegment() >= EndOfLeftBorder && position_.subsegment() < EndOfPixels;

	return
		0x01 |		// Leave lower bit for population elsewhere
		(b5 ? 0x00 : 0x20) |
		(b7 ? 0x00 : 0x80);
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
	if(line_end == CyclesPerLine) {
		crt_.output_sync(CyclesPerLine); //line_end - line_begin);
	}
}

void Video::border_line(const int line_begin, const int line_end) {
	Numeric::clamp<0, EndOfSync>(line_begin, line_end, [&](const int begin, const int end) {
		crt_.output_sync(end - begin);
	});
	Numeric::clamp<EndOfSync, CyclesPerLine>(line_begin, line_end, [&](const int begin, const int end) {
		crt_.output_level(end - begin, border_);
	});
}

void Video::pixel_line(const int line_begin, const int line_end) {
	Numeric::clamp<0, EndOfSync>(line_begin, line_end, [&](const int begin, const int end) {
		crt_.output_sync(end - begin);
	});
	Numeric::clamp<EndOfSync, EndOfLeftBorder>(line_begin, line_end, [&](const int begin, const int end) {
		crt_.output_level(end - begin, border_);
	});
	Numeric::clamp<EndOfLeftBorder, EndOfPixels>(line_begin, line_end, [&](const int begin, const int end) {
		if(begin == EndOfLeftBorder) {
			output_ = reinterpret_cast<uint16_t *>(crt_.begin_data(PixelsPerLine));
		}

		if(output_) {
			for(int c = begin; c < end; c++) {
				const uint8_t pixels = pixels_[source_address_];
				const uint8_t attributes = attributes_[source_address_];
				++source_address_;

				const uint16_t colours[] = {
					palette[attributes & 0xf],
					palette[attributes >> 4],
				};

				output_[0] = colours[(pixels >> 7) & 1];
				output_[1] = colours[(pixels >> 6) & 1];
				output_[2] = colours[(pixels >> 5) & 1];
				output_[3] = colours[(pixels >> 4) & 1];
				output_[4] = colours[(pixels >> 3) & 1];
				output_[5] = colours[(pixels >> 2) & 1];
				output_[6] = colours[(pixels >> 1) & 1];
				output_[7] = colours[(pixels >> 0) & 1];
				output_ += 8;
			}
		} else {
			source_address_ += end - begin;
		}

		if(end == EndOfPixels) {
			crt_.output_data(EndOfPixels - EndOfLeftBorder, PixelsPerLine);
			output_ = nullptr;
		}
	});
	Numeric::clamp<EndOfPixels, CyclesPerLine>(line_begin, line_end, [&](const int begin, const int end) {
		crt_.output_level(end - begin, border_);
	});
}

Cycles Video::next_sequence_point() const {
	// Pulse the interrupt output for 8 cycles, arbitrarily. The real number seems to be undocumented, and it
	// doesn't actually make much concrete difference in concrete terms because this is fed into edge-detection via
	// the PIA. Knowing the real number would only fix the case where the detected transition is switched to
	// trailing-edge during the pulse, which probably doesn't happen in any real software.
	//
	// Would be nice to know the real number though.
	if(position_.absolute() < IRQCycle) return IRQCycle - position_.absolute();
	if(position_.absolute() < IRQCycle + IRQLength) return IRQCycle + IRQLength - position_.absolute();
	return FrameLength - position_.absolute() + IRQCycle;
}

bool Video::irq() const {
	return position_.absolute() >= IRQCycle && position_.absolute() < (IRQCycle + IRQLength);
}

void Video::set_border_colour(const uint8_t colour) {
	border_ = palette[colour & 0xf];
}
