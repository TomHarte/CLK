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
#include "Numeric/SizedInt.hpp"

namespace Motorola::MC6847 {

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

struct MC6847Delegate {
	virtual void set_hsync(bool active) = 0;
	virtual void set_vsync(bool active) = 0;
};

struct MC6847Base {
	MC6847Base(Outputs::Display::Type);

	void pixel_line(int begin, int end);
	void border_line(int begin, int end);
	void porch_line(int begin, int end);
	void sync_line(int begin, int end);
	void reset();

	struct LineLayout {
		// Start of line: sync is active.
		static const int EndOfSync = 28;
		static const int EndOfColourBurst = 55;
		static const int EndOfBackPorch = 72;
		static const int EndOfLeftBorder = 130;
		static const int EndOfPixels = 386;
		static const int EndOfRightBorder = 442;
		static const int EndOfLine = 456;
	};

	Outputs::CRT::CRT crt_;
	uint16_t *pixels_ = nullptr;

	struct LineCapture {
		// TODO: capture mode here.
		uint8_t data[32];
	} line_;

	struct Address {
	public:
		uint16_t address() const {
			return address_;
		}

		int row() const {
			return row_.get();
		}

		void advance(int column);
		void apply_vertical_preload();
		void apply_hsync();

	private:
		uint16_t address_ = 0;

		uint16_t increment_mask_ = 1;
		uint16_t line_mask_ = uint16_t(~31);

		Numeric::SizedInt<4> row_;
		uint8_t target_row_ = 12;
	} address_;
};

template <FrameTiming timing, typename MemoryAccessT>
class MC6847: private MC6847Base {
public:
	MC6847(MemoryAccessT &memory) :
		MC6847Base(is_ntsc(timing) ? Outputs::Display::Type::NTSC60 : Outputs::Display::Type::PAL50),
		memory_(memory) {
		crt_.set_fixed_framing([&] {
			run_for(Cycles(10'000));
		});
	}

	//
	// Expected input clock: double NTSC 3.58 Mhz; i.e. supply the pixel clock directly.
	//
	void run_for(const Cycles cycles) {
		// This template binds the drawing logic to whatever was supplied
		// as MemoryAccessT; a consequence of that is that frame layout logic is here.
		position_.advance(
			cycles.as<int>(),
			[&] (const int line, const int begin, const int end) {
				if(line < FrameLayout<timing>::EndOfPixels) {
					Numeric::clamp<LineLayout::EndOfLeftBorder, LineLayout::EndOfPixels>(
						begin,
						end,
						[&](const int fetch_begin, const int fetch_end) {
							const int column_begin = (fetch_begin - LineLayout::EndOfLeftBorder) >> 3;
							const int column_end = (fetch_end - LineLayout::EndOfLeftBorder) >> 3;

							for(int c = column_begin; c < column_end; c++) {
								const auto source = address_.address() + 0x400;	// TODO: unhack.
								line_.data[c] = memory_[size_t(source)];
								address_.advance(c);
							}
						}
					);
					pixel_line(begin, end);
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
	Numeric::DividingAccumulator<LineLayout::EndOfLine, FrameLayout<timing>::EndOfField> position_{2};
};

}
