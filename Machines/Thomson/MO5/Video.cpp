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

constexpr int CyclesPerLine = 64;

constexpr int TotalPixelLines = 200;
constexpr int TotalLines = 312;
constexpr int VerticalSyncLine = 256;

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

	// HACK! Go line by line.
	while(to_run) {
		// HACK! Coarse counting block: work out number of lines to process.
		const int start = position_ / CyclesPerLine;
		const int end = std::min((position_ + to_run) / CyclesPerLine, TotalLines);
		if(start == end) {
			position_ += to_run;
			break;
		}

		for(int line = start; line < end; line++) {
			if(line >= VerticalSyncLine && line < VerticalSyncLine + 3) {
				crt_.output_sync(CyclesPerLine);
				continue;
			}

			if(line >= TotalPixelLines) {
				crt_.output_sync(4);
				crt_.output_level(60, border_);
				continue;
			}

			crt_.output_sync(4);
			crt_.output_level(10, border_);

			output_ = reinterpret_cast<uint16_t *>(crt_.begin_data(320));
			if(output_) {
				for(int x = 0; x < 40; x++) {
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
				source_address_ += 40;
			}

			crt_.output_data(40, 320);
			crt_.output_level(10, border_);
		}

		// HACK! Assuming the above proceeded in lines, adjust for number of cycles consumed.
		const int did_run = (CyclesPerLine - (position_ % CyclesPerLine)) + ((end - start) - 1) * CyclesPerLine;
		to_run -= did_run;
		position_ += did_run;
		if(position_ == FrameLength) {
			position_ = 0;
			source_address_ = 0;
		}
	}
}

Cycles Video::next_sequence_point() const {
	// Pulse the interrupt output for 8 cycles, arbitrarily. TODO: what's the real number?
	if(position_ < IRQCycle) return IRQCycle - position_;
	if(position_ < IRQCycle + IRQLength) return IRQCycle + IRQLength - position_;
	return FrameLength - position_ + IRQCycle;
}

bool Video::irq() const {
	return position_ >= IRQCycle && position_ < (IRQCycle + IRQLength);
}

void Video::set_border_colour(const uint8_t colour) {
	// TODO: bits possibly need a swizzle?
	border_ = palette[colour & 0xf];
}
