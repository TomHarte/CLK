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

// Video timing, as far as auto-translate lets me figure it out:
//
//	64 cycles/line;
//	56 lines post signalled vsync, then 200 of video, then 56 more, for 312 total.
//
// Start of vsync is connected to CPU IRQ.
//
// Within a line: ??? Who knows ???
//
// Have rationalised as 4 cycles of sync and the rest as appropriate colours. Via IRQCycle the interrupt can be placed
// arbitrarily within the frame so I think any implementation within a line is valid as long as I place the interrupt
// appropriately. TODO: where is the interrupt placed?
//

constexpr int CyclesPerLine = 64;

constexpr int TotalPixelLines = 200;
constexpr int TotalLines = 312;
constexpr int VerticalSyncLine = 256;
constexpr int VerticalSyncLength = 3;

constexpr int IRQCycle = 256 * CyclesPerLine;
constexpr int IRQLength = 8;

constexpr int FrameLength = TotalLines * CyclesPerLine;

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

/// Calculates the overlap, if any, between [begin, end] and [RangeBegin, RangeEnd] and calls FuncT with a begin and end if the range is non-zero.
template <const int RangeBegin, const int RangeEnd, typename FuncT>
void clamp(const int begin, const int end, const FuncT &&function) {
	const int range_begin = std::max(begin, RangeBegin);
	const int range_end = std::min(end, RangeEnd);
	if(range_end > range_begin) {
		function(range_begin, range_end);
	}
}

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
	int to_run = cycles.as<int>();

	while(to_run) {
		const int line = position_ / CyclesPerLine;
		const int start_column = position_ % CyclesPerLine;

		const int in_line = std::min(CyclesPerLine - start_column, to_run);
		const int end_column = std::min(CyclesPerLine, start_column + in_line);
		to_run -= in_line;

		if(position_ == FrameLength) {
			position_ = 0;
			source_address_ = 0;
		}
		position_ += in_line;

		if(line >= VerticalSyncLine && line < VerticalSyncLine + VerticalSyncLength) {
			vsync_line(start_column, end_column);
		} else if(line >= TotalPixelLines) {
			border_line(start_column, end_column);
		} else {
			pixel_line(start_column, end_column);
		}
	}
}

void Video::vsync_line(const int line_begin, const int line_end) {
	crt_.output_sync(line_end - line_begin);
}

void Video::border_line(const int line_begin, const int line_end) {
	static constexpr int EndOfSync = 4;
	clamp<0, EndOfSync>(line_begin, line_end, [&](const int begin, const int end) {
		crt_.output_sync(end - begin);
	});
	clamp<EndOfSync, CyclesPerLine>(line_begin, line_end, [&](const int begin, const int end) {
		crt_.output_level(end - begin, border_);
	});
}

void Video::pixel_line(const int line_begin, const int line_end) {
	// Layout: [sync][border][pixels][border].
	static constexpr int EndOfSync = 4;
	static constexpr int EndOfLeftBorder = 14;
	static constexpr int EndOfPixels = 54;

	clamp<0, EndOfSync>(line_begin, line_end, [&](const int begin, const int end) {
		crt_.output_sync(end - begin);
	});
	clamp<EndOfSync, EndOfLeftBorder>(line_begin, line_end, [&](const int begin, const int end) {
		crt_.output_level(end - begin, border_);
	});
	clamp<EndOfLeftBorder, EndOfPixels>(line_begin, line_end, [&](const int begin, const int end) {
		if(begin == EndOfLeftBorder) {
			output_ = reinterpret_cast<uint16_t *>(crt_.begin_data(320));
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
			crt_.output_data(40, 320);
			output_ = nullptr;
		}
	});
	clamp<EndOfPixels, CyclesPerLine>(line_begin, line_end, [&](const int begin, const int end) {
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
	if(position_ < IRQCycle) return IRQCycle - position_;
	if(position_ < IRQCycle + IRQLength) return IRQCycle + IRQLength - position_;
	return FrameLength - position_ + IRQCycle;
}

bool Video::irq() const {
	return position_ >= IRQCycle && position_ < (IRQCycle + IRQLength);
}

void Video::set_border_colour(const uint8_t colour) {
	border_ = palette[colour & 0xf];
}
