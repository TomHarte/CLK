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

// Notes:
//
// # https://www.acornatom.nl/sites/atomreview/howel/logic/6847_clone.htm:
//
// The master clock was designed to run at 3.579545 MHz, because they are used in NTSC TVs and so are very cheap.
// This is also useful for creating NTSC-compatible colour phase signals. Internally the 6847 divides the clock by 3.5
// to get a frame timing frequency of 1.022727 MHz. This is divided by 64 to get the line rate.
//
// Each line has up to 256 pixels from 32 memory accesses. There are 8 pixels per memory access.

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

struct MC6847Base {
	MC6847Base(const Outputs::Display::Type display_type) :
		crt_(
		64,
		1,
		display_type,
		Outputs::Display::InputDataType::Red4Green4Blue4	// TODO.
	) {}

	void pixel_line(int begin, int end);	// TODO: require fetched data to be supplied.
	void border_line(int begin, int end);
	void porch_line(int begin, int end);
	void sync_line(int begin, int end);

	struct LineLayout {
		// Complete guesses. Who knows?
		static const int EndOfSync = 4;
		static const int EndOfLeftBorder = 18;
		static const int EndOfPixels = 50;
		static const int EndOfLine = 64;
	};

	Outputs::CRT::CRT crt_;
};

template <FrameTiming timing, typename MemoryAccessT>
class MC6847: private MC6847Base {
public:
	MC6847(MemoryAccessT &memory) :
		MC6847Base(is_ntsc(timing) ? Outputs::Display::Type::NTSC60 : Outputs::Display::Type::PAL50),
		memory_(memory) {}

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
				// TODO: reset counters, etc.
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
