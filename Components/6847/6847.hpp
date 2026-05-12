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

// TODO: allow a delegate to be specified, and signal it.
struct MC6847Delegate {
	virtual void set_hsync(bool active) = 0;
	virtual void set_vsync(bool active) = 0;
	virtual void set_row_preset(bool active) = 0;
};

namespace Mode {
using IntT = uint8_t;

constexpr IntT Semigraphics	= 0b1000'0000;
constexpr IntT Graphics		= 0b0100'0000;
constexpr IntT ExternalROM	= 0b0010'0000;
constexpr IntT Invert		= 0b0001'0000;
constexpr IntT ColourSelect	= 0b0000'1000;
// ... 0b0000'0100 unused ...
constexpr IntT BPP2			= 0b0000'0010;
constexpr IntT Columns16	= 0b0000'0001;

/// Maps between [GM2, GM1, GM0] and named mode.
enum GraphicsMode: IntT {
	ColourGraphics1 = 0b000,			// 64x64 @ 2bpp; 1kb required.
	ResolutionGraphics1 = 0b001,		// 128x64 @ 1bpp; 1kb required.

	ColourGraphics2 = 0b010,			// 128x64 @ 2bpp: 2kb required.
	ResolutionGraphics2 = 0b011,		// 128x96 @ 1bpp; 2kb required.

	ColourGraphics3 = 0b100,			// 128x96 @ 2bpp; 3kb required.
	ResolutionGraphics3 = 0b101,		// 128x192 @ 1bpp; 3kb required.

	ColourGraphics6 = 0b110,			// 128x192 @ 1bpp; 6kb required.
	ResolutionGraphics6 = 0b111,		// 256x192 @ 1bpp; 6kb required.
};

constexpr bool is_16column(const GraphicsMode mode) {
	switch(mode) {
		using enum Mode::GraphicsMode;

		case ResolutionGraphics1:
		case ResolutionGraphics2:
		case ResolutionGraphics3:
		case ColourGraphics1:
			return true;

		default: return false;
	}
}

constexpr uint8_t is_2bpp(const GraphicsMode mode) {
	switch(mode) {
		using enum Mode::GraphicsMode;

		case ColourGraphics1:
		case ColourGraphics2:
		case ColourGraphics3:
		case ColourGraphics6:
			return true;

		default: return false;
	}
}

constexpr uint8_t repeated_rows(const GraphicsMode mode) {
	switch(mode) {
		using enum Mode::GraphicsMode;

		case ResolutionGraphics1:
		case ColourGraphics1:
		case ColourGraphics2:
			return 3;

		case ResolutionGraphics2:
		case ColourGraphics3:
			return 2;

		default: return 1;
	}
}

}

struct MC6847Base {
	MC6847Base(Outputs::Display::Type);

	void pixel_line(int begin, int end);
	void border_line(int begin, int end);
	void porch_line(int begin, int end);
	void sync_line(int begin, int end);
	void reset();
	bool hsync(int column) const;
	Cycles next_sequence_point(int column) const;

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

	void set_mode(
		bool graphics,
		bool semigraphics,
		bool external_rom,
		bool invert,
		uint8_t graphics_mode,
		const bool colour_select
	);
	Mode::IntT mode_;

	struct LineCapture {
		struct Column {
			uint8_t data;
			Mode::IntT mode;
		} columns[32];
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

		uint16_t increment_mask_ = 1;
		uint16_t line_mask_ = uint16_t(~31);
		uint8_t target_row_ = 12;

	private:
		uint16_t address_ = 0;
		Numeric::SizedInt<4> row_;
	} address_;
};

struct NullMapper {
	Mode::IntT operator()(const uint8_t, const Mode::IntT mode) {
		return mode;
	}
};

template <FrameTiming timing, typename MemoryAccessT, typename ModeMapperT = NullMapper>
class MC6847: private MC6847Base {
public:
	MC6847(MemoryAccessT &);

	void run_for(const Cycles);
	Cycles next_sequence_point() const;

	bool hsync() const;
	bool vsync() const;
	using MC6847Base::set_mode;

	// MARK: - ScanTarget entrypoints.

	void set_scan_target(Outputs::Display::ScanTarget *);
	Outputs::Display::ScanStatus get_scaled_scan_status() const;
	void set_display_type(const Outputs::Display::DisplayType);
	Outputs::Display::DisplayType get_display_type() const;

private:
	MemoryAccessT &memory_;
	Numeric::DividingAccumulator<LineLayout::EndOfLine, FrameLayout<timing>::EndOfField> position_{2};
};

// MARK: - Implementation.

template <FrameTiming timing, typename MemoryAccessT, typename ModeMapperT>
MC6847<timing, MemoryAccessT, ModeMapperT>::MC6847(MemoryAccessT &memory) :
	MC6847Base(is_ntsc(timing) ? Outputs::Display::Type::NTSC60 : Outputs::Display::Type::PAL50),
	memory_(memory) {
	crt_.set_fixed_framing([&] {
		run_for(Cycles(10'000));
	});
}

template <FrameTiming timing, typename MemoryAccessT, typename ModeMapperT>
void MC6847<timing, MemoryAccessT, ModeMapperT>::run_for(const Cycles cycles) {
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
							const auto source = address_.address();
							line_.columns[c].data = memory_[source];
							line_.columns[c].mode = ModeMapperT()(line_.columns[c].data, mode_);
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

template <FrameTiming timing, typename MemoryAccessT, typename ModeMapperT>
Cycles MC6847<timing, MemoryAccessT, ModeMapperT>::next_sequence_point() const {
	return MC6847Base::next_sequence_point(position_.subsegment());
}

template <FrameTiming timing, typename MemoryAccessT, typename ModeMapperT>
bool MC6847<timing, MemoryAccessT, ModeMapperT>::hsync() const {
	return MC6847Base::hsync(position_.subsegment());
}

template <FrameTiming timing, typename MemoryAccessT, typename ModeMapperT>
bool MC6847<timing, MemoryAccessT, ModeMapperT>::vsync() const {
	const auto segment = position_.segment();
	return segment >= FrameLayout<timing>::EndOfFrontPorch && segment < FrameLayout<timing>::EndOfSync;
}

template <FrameTiming timing, typename MemoryAccessT, typename ModeMapperT>
void MC6847<timing, MemoryAccessT, ModeMapperT>::set_scan_target(Outputs::Display::ScanTarget *const target) {
	crt_.set_scan_target(target);
}

template <FrameTiming timing, typename MemoryAccessT, typename ModeMapperT>
Outputs::Display::ScanStatus MC6847<timing, MemoryAccessT, ModeMapperT>::get_scaled_scan_status() const {
	return crt_.get_scaled_scan_status();
}

template <FrameTiming timing, typename MemoryAccessT, typename ModeMapperT>
void MC6847<timing, MemoryAccessT, ModeMapperT>::set_display_type(const Outputs::Display::DisplayType display_type) {
	crt_.set_display_type(display_type);
}

template <FrameTiming timing, typename MemoryAccessT, typename ModeMapperT>
Outputs::Display::DisplayType MC6847<timing, MemoryAccessT, ModeMapperT>::get_display_type() const {
	return crt_.get_display_type();
}

}
