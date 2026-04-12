//
//  Video.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/03/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#ifndef Video_hpp
#define Video_hpp

#include "ClockReceiver/ClockReceiver.hpp"
#include "Numeric/SegmentCounter.hpp"
#include "Outputs/CRT/CRT.hpp"

namespace Thomson::MO5 {

struct Video {
public:
	Video(const uint8_t *pixels, const uint8_t *attributes);
	void run_for(Cycles);
	Cycles next_sequence_point() const;

	void set_border_colour(uint8_t);
	bool irq() const;

	uint8_t vertical_state() const;
	uint8_t horizontal_state() const;

	// MARK: - Standard boilerplate.

	void set_scan_target(Outputs::Display::ScanTarget *const target) {
		crt_.set_scan_target(target);
	}

	Outputs::Display::ScanStatus get_scaled_scan_status() const {
		return crt_.get_scaled_scan_status();
	}

	void set_display_type(const Outputs::Display::DisplayType display_type) {
		crt_.set_display_type(display_type);
	}

	Outputs::Display::DisplayType get_display_type() const {
		return crt_.get_display_type();
	}

private:
	const uint8_t *pixels_ = nullptr;
	const uint8_t *attributes_ = nullptr;
	Outputs::CRT::CRT crt_;

	uint16_t source_address_ = 0;
	uint16_t border_ = 0;
	uint16_t *output_ = nullptr;

	void vsync_line(int, int);
	void border_line(int, int);
	void pixel_line(int, int);

	// Total frame size.
	static constexpr int CyclesPerLine = 64;
	static constexpr int TotalLines = 312;
	static constexpr int FrameLength = TotalLines * CyclesPerLine;

	// Line placement; pixel lines begin with internal line 0.
	static constexpr int TotalPixelLines = 200;
	static constexpr int VerticalSyncLine = 256;
	static constexpr int VerticalSyncLength = 3;

	// IRQ placement.
	static constexpr int IRQCycle = 256 * CyclesPerLine;
	static constexpr int IRQLength = 8;

	// Line layout: [sync][border][pixels][border].
	static constexpr int EndOfSync = 4;
	static constexpr int EndOfLeftBorder = 14;
	static constexpr int EndOfPixels = 54;
	static constexpr int PixelsPerLine = 320;

	// Frame position counter.
	Numeric::DividingAccumulator<CyclesPerLine, TotalLines> position_;
};

}

#endif /* Video_hpp */
