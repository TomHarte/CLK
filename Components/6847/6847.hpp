//
//  6847.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/05/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include "ClockReceiver/ClockReceiver.hpp"
#include "Numeric/SegmentCounter.hpp"
#include "Outputs/CRT/CRT.hpp"

namespace Motorola::MC6847 {

/*!
	Encapsulates the 6847's address-generation logic;
*/
struct AddressGenerator {
	// MARK: - Events.

	void apply_hsync() {
		if(mode_ == 7) [[unlikely]] {
			return;
		}

		// Use y divider to decide whether to clear low bits.
		++y_;
		if(y_ == y_divider_ || y_ == 12) {	// Complete guess: always wrap at 12 even if an in-frame mode switch
											// has occurred.
			y_ = 0;
			++counter_;
		}

		// Clear either four or five bits.
		if(mode_ & 1) {
			counter_ &= ~15;
		} else {
			counter_ &= ~31;
		}
	}

	void apply_vertical_preload() {
		x_ = 0;
		y_ = 0;
		counter_ = preload_;
	}

	void advance() {
		++x_;
		if(x_ == x_divider_ || x_ == 3) {
			++counter_;
			x_ = 0;
		}
	}

	uint16_t address() const {
		return counter_;
	}

	int row() const {
		return y_;
	}

	// MARK: - Inputs.

	void set_hsync(const bool active) {
		if(active != previous_hsync_) [[unlikely]] {
			previous_hsync_ = active;
			if(active) apply_hsync();
		}
	}

	void set_mode(const int mode) {
		mode_ = mode & 7;

		switch(mode) {
			case 0:
				x_divider_ = 1;
				y_divider_ = 12;
			break;
			case 1:
				x_divider_ = 3;
				y_divider_ = 1;
			break;
			case 2:
				x_divider_ = 1;
				y_divider_ = 3;
			break;
			case 3:
				x_divider_ = 2;
				y_divider_ = 1;
			break;
			case 4:
				x_divider_ = 1;
				y_divider_ = 2;
			break;
			default:
				x_divider_ = 1;
				y_divider_ = 2;
			break;
		}
	}

	// TODO: "VP goes active (high) when HS from the VDG rises if DAO is high (or a high impedance.)"

	// MARK: - Helpers, as this class is intended to straddle a few uses cases.

	void set_preload(const uint16_t preload) {
		preload_ = preload;
	}

private:
	uint16_t counter_ = 0;
	uint16_t preload_ = 0;

	bool previous_hsync_ = false;
	int mode_ = 0;

	int x_divider_= 1;
	int y_divider_= 1;
	int x_ = 0;
	int y_ = 0;
};

enum class FrameTiming {
	/// Official frame timing, as implemented by the chip.
	NTSC,

	/// Standard PAL adaptation of adding 25 lines before vsync and 25 aftter, as used independently by
	/// the Acorn Atom and Dragon 32.
	PALPaddedVsync,
};
constexpr bool is_ntsc(const FrameTiming timing) {
	return timing == FrameTiming::NTSC;
}
template <FrameTiming timing> struct FrameLayout {
	static constexpr int BorderLength = timing == FrameTiming::NTSC ? 27 : 52;

	static constexpr int EndOfPixels = 192;
	static constexpr int EndOfBottomBorder = EndOfPixels + BorderLength;
	static constexpr int EndOfFrontPorch = EndOfBottomBorder + 2;
	static constexpr int EndOfSync = EndOfFrontPorch + 2;
	static constexpr int EndOfBackPorch = EndOfSync + 12;
	static constexpr int EndOfField = EndOfBackPorch + BorderLength;
};
static_assert(FrameLayout<FrameTiming::NTSC>::EndOfField == 262, "NTSC frames should be 262 lines long");
static_assert(FrameLayout<FrameTiming::PALPaddedVsync>::EndOfField == 312, "PAL-padded frames should be 312 lines long");

struct MC6847Base {
	MC6847Base(Outputs::Display::Type);

	void pixel_line(int line, int begin, int end);
	void border_line(int begin, int end);
	void porch_line(int begin, int end);
	void sync_line(int begin, int end);
	void reset();

	struct LineLayout {
		// Complete guesses. Who knows?
		static const int EndOfSync = 4;
		static const int EndOfLeftBorder = 18;
		static const int EndOfPixels = 50;
		static const int EndOfLine = 64;
	};

	Outputs::CRT::CRT crt_;
	uint16_t *pixels_ = nullptr;

	struct LineCapture {
		// TODO: capture mode here.
		uint8_t data[32];
	} line_;

	AddressGenerator address_;
};

template <FrameTiming timing, typename MemoryAccessT>
class MC6847: private MC6847Base {
public:
	MC6847(MemoryAccessT &memory) :
		MC6847Base(is_ntsc(timing) ? Outputs::Display::Type::NTSC60 : Outputs::Display::Type::PAL50),
		memory_(memory) {
		// HACK. Allows me to test the address generator locally, prior to integrating it into the SAM.
		address_.set_preload(0x400);
		address_.set_mode(0);

		crt_.set_fixed_framing([&] {
			run_for(Cycles(10'000));
		});
	}

	//
	// Expected input clock: NTSC-style 3.58 Mhz.
	//
	void run_for(const Cycles cycles) {
		// This template binds the drawing logic to whatever was supplied
		// as MemoryAccessT; a consequence of that is that frame layout logic is here.
		cycles_ += cycles;
		position_.advance(
			cycles_.divide(2).template as<int>(),
			[&] (const int line, const int begin, const int end) {
				if(line < FrameLayout<timing>::EndOfPixels) {
					Numeric::clamp<LineLayout::EndOfLeftBorder, LineLayout::EndOfPixels>(
						begin,
						end,
						[&](const int fetch_begin, const int fetch_end) {
							for(int c = fetch_begin; c < fetch_end; c++) {
								if(c > LineLayout::EndOfLeftBorder) address_.advance();
								const int column = c - LineLayout::EndOfLeftBorder;
								const auto source = address_.address();
								line_.data[column] = memory_[source];
							}
						}
					);
					pixel_line(line, begin, end);
				} else if(line < FrameLayout<timing>::EndOfBottomBorder) {
					border_line(begin, end);
				} else if(line < FrameLayout<timing>::EndOfFrontPorch) {
					porch_line(begin, end);
				} else if(line < FrameLayout<timing>::EndOfSync) {
					sync_line(begin, end);
				} else if(line < FrameLayout<timing>::EndOfBackPorch) {
					porch_line(begin, end);
				} else {
					border_line(begin, end);
				}
			},
			[&] {
				reset();
			}
		);
	}

	void set_scan_target(Outputs::Display::ScanTarget *const target) {
		crt_.set_scan_target(target);
	}

	Outputs::Display::ScanStatus get_scaled_scan_status() const {
		return crt_.get_scaled_scan_status() * 2;
	}

	void set_display_type(const Outputs::Display::DisplayType display_type) {
		crt_.set_display_type(display_type);
	}

	Outputs::Display::DisplayType get_display_type() const {
		return crt_.get_display_type();
	}

private:
	MemoryAccessT &memory_;
	Cycles cycles_;
	Numeric::DividingAccumulator<64, FrameLayout<timing>::EndOfField> position_;
};

}
