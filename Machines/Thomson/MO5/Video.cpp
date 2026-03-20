//
//  Video.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/03/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include "Video.hpp"

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

static constexpr int CyclesPerLine = 64;

static constexpr int TotalPixelLines = 200;
static constexpr int TotalLines = 312;
static constexpr int VerticalSyncLine = 256;

static constexpr int IRQCycle = 256 * CyclesPerLine;
static constexpr int FrameLength = TotalLines * CyclesPerLine;
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
			if(line >= VerticalSyncLine && line < VerticalSyncLine * 3) {
				crt_.output_sync(CyclesPerLine);
				continue;
			}

			if(line >= TotalPixelLines) {
				crt_.output_sync(4);
				crt_.output_blank(60);
				continue;
			}

			crt_.output_sync(4);
			crt_.output_blank(10);

			output_ = reinterpret_cast<uint16_t *>(crt_.begin_data(320));
			if(output_) {
				for(int x = 0; x < 40; x++) {
					const uint8_t pixels = pixels_[source_address_++];

					output_[0] = (pixels & 0x80) ? 0xffff : 0x0000;
					output_[1] = (pixels & 0x40) ? 0xffff : 0x0000;
					output_[2] = (pixels & 0x20) ? 0xffff : 0x0000;
					output_[3] = (pixels & 0x10) ? 0xffff : 0x0000;
					output_[4] = (pixels & 0x08) ? 0xffff : 0x0000;
					output_[5] = (pixels & 0x04) ? 0xffff : 0x0000;
					output_[6] = (pixels & 0x02) ? 0xffff : 0x0000;
					output_[7] = (pixels & 0x01) ? 0xffff : 0x0000;
					output_ += 8;
				}
			}

			crt_.output_data(40, 320);
			crt_.output_blank(10);
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
