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
	static constexpr int CyclesPerLine = 64;
	static constexpr int TotalLines = 312;

	static constexpr int TotalPixelLines = 200;
	static constexpr int VerticalSyncLine = 256;
	static constexpr int VerticalSyncLength = 3;

	static constexpr int IRQCycle = 256 * CyclesPerLine;
	static constexpr int IRQLength = 8;

	static constexpr int FrameLength = TotalLines * CyclesPerLine;

	Numeric::DividingAccumulator<CyclesPerLine, TotalLines> position_;
};

}

#endif /* Video_hpp */
