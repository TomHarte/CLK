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
#include "Numeric/SizedInt.hpp"
#include "Outputs/CRT/CRT.hpp"

namespace Thomson::MO5 {

struct Video {
public:
	Video(const uint8_t *pixels, const uint8_t *attributes);
	void run_for(Cycles);
	Cycles next_sequence_point() const;
	bool irq() const;

	// MARK: - Writes.

	void set_border_colour(uint8_t);
	void set_palette_index(uint8_t);
	void set_palette(uint8_t);
	void set_mode(uint8_t);

	// MARK: - Reads.

	uint8_t vertical_state() const;
	uint8_t horizontal_state() const;
	uint8_t palette_index() const;
	uint8_t palette();

	// MARK: - Address-based access.

	template <uint16_t address>
	uint8_t read() {
		switch(address) {
			case 0xa7e4:	return 0;	// TODO: MSB of lightpen counter.
			case 0xa7e5:	return 0;	// TODO: LSB of lightpen counter.
			case 0xa7e6:	return horizontal_state();
			case 0xa7e7:	return vertical_state();

			case 0xa7da:	return palette();
			case 0xa7db:	return palette_index();

			default:		break;
		}
		return 0xff;
	}

	template <uint16_t address>
	void write(const uint8_t value) {
		switch(address) {
			case 0xa7e5:
				// TODO: enable (0) or disable (1) video mode access at 0xa7dc.
			break;
			case 0xa7e7:
				// TODO: b5 = 1 => 525-line output; 0 = 625-line output.
			break;

			case 0xa7da:	set_palette(value);			break;
			case 0xa7db:	set_palette_index(value);	break;
			case 0xa7dc:	set_mode(value);			break;
			case 0xa7dd:	set_border_colour(value);	break;

			default:
			break;
		}
	}

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
	uint16_t *output_ = nullptr;

	uint16_t mapped_palette_[16];
	uint16_t mapped_border_ = 0;
	void update_mapped_border() {
		mapped_border_ = mapped_palette_[border_];
	}

	// Pixel outputters.
	void vsync_line(int, int);
	void border_line(int, int);
	template <uint8_t mode>
	void pixel_line(int, int);

	// For gate array support: palette and mode.
	uint8_t palette_[32];
	uint8_t border_;
	Numeric::SizedInt<5> palette_index_ = 0;
	uint8_t mode_ = 0b0'01'00'000;	// Direct mode byte translation, 8 Mhz clock, 40-column output.

	// Line layout: [sync][border][pixels][border].
	struct Line {
		static constexpr int EndOfSync = 4;
		static constexpr int EndOfLeftBorder = 14;
		static constexpr int EndOfPixels = 54;
		static constexpr int TotalCycles = 64;

		static constexpr int TotalPixels = 320;
	};

	// Total frame size.
	struct Frame {
		static constexpr int TotalLines = 312;
		static constexpr int TotalCycles = TotalLines * Line::TotalCycles;

		// Line placement; pixel lines begin with internal line 0.
		static constexpr int TotalPixelLines = 200;
		static constexpr int VerticalSyncLine = 256;
		static constexpr int VerticalSyncLength = 3;
	};

	// IRQ placement.
	struct IRQ {
		static constexpr int Cycle = 256 * Line::TotalCycles;
		static constexpr int Length = 8;
	};

	// Frame position counter.
	Numeric::DividingAccumulator<Line::TotalCycles, Frame::TotalLines> position_;
};

}

#endif /* Video_hpp */
