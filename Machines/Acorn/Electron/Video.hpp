//
//  Video.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/12/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#pragma once

#include "Outputs/CRT/CRT.hpp"
#include "ClockReceiver/ClockReceiver.hpp"
#include "Interrupts.hpp"

#include <utility>
#include <vector>

namespace Electron {

/*!
	Implements the Electron's video subsystem plus appropriate signalling.

	The Electron has an interlaced fully-bitmapped display with six different output modes,
	running either at 40 or 80 columns. Memory is shared between video and CPU; when the video
	is accessing it the CPU may not.
*/
class VideoOutput {
public:
	/*!
		Instantiates a VideoOutput that will read its pixels from @c memory.

		The pointer supplied should be to address 0 in the unexpanded Electron's memory map.
	*/
	VideoOutput(const uint8_t *memory);

	/// Sets the destination for output.
	void set_scan_target(Outputs::Display::ScanTarget *);

	/// Gets the current scan status.
	Outputs::Display::ScanStatus get_scaled_scan_status() const;

	/// Sets the type of output.
	void set_display_type(Outputs::Display::DisplayType);

	/// Gets the type of output.
	Outputs::Display::DisplayType get_display_type() const;

	/// Produces the next @c cycles of video output.
	///
	/// @returns a bit mask of all interrupts triggered.
	uint8_t run_for(const Cycles);

	/// Runs for as many cycles as is correct to get to the next RAM access slot.
	///
	/// @returns the number of cycles run for and a bit mask of all interrupts triggered.
	std::pair<Cycles, uint8_t> run_until_ram_slot();

	/// Runs for as many cycles as is correct to get to the next IO access slot.
	///
	/// @returns the number of cycles run for and a bit mask of all interrupts triggered.
	std::pair<Cycles, uint8_t> run_until_io_slot();

	/*!
		Writes @c value to the register at @c address. May mutate the results of @c get_next_interrupt,
		@c get_cycles_until_next_ram_availability and @c get_memory_access_range.
	*/
	void write(const int address, const uint8_t value);

	/*!
		@returns the number of cycles after (final cycle of last run_for batch + @c from_time)
		before the video circuits will allow the CPU to access RAM.
	*/
	unsigned int get_cycles_until_next_ram_availability(const int from_time);

private:
	const uint8_t *const ram_ = nullptr;

	// CRT output.
	enum class OutputStage {
		Sync, Blank, Pixels, ColourBurst,
	};
	OutputStage output_ = OutputStage::Blank;
	int output_length_ = 0;
	int screen_pitch_ = 0;

	uint8_t *current_output_target_ = nullptr;
	uint8_t *initial_output_target_ = nullptr;
	int current_output_divider_ = 1;
	Outputs::CRT::CRT crt_;

	// Palettes.
	uint8_t source_palette_[8]{};
	uint8_t mapped_palette_[16]{};

	struct BitIndex {
		int address;
		int bit;
	};

	template <BitIndex index, int target_bit>
	requires (
		target_bit >= 0 && target_bit <= 2 &&
		index.bit >= 0 && index.bit <= 7 &&
		index.address >= 0xfe08 && index.address <= 0xfe0f
	)
	uint8_t channel() {
		return uint8_t(((source_palette_[index.address - 0xfe08] >> index.bit) & 1) << target_bit);
	}

	template <BitIndex red, BitIndex green, BitIndex blue>
	uint8_t palette_entry() {
		return channel<red, 2>() | channel<green, 1>() | channel<blue, 0>();
	}

	template <uint16_t pair, int base>
	requires ((pair & 1) == 0 && pair >= 0xfe08 && pair <= 0xfe0e && base >= 0 && base < 16 && !(base & 0b1010))
	void set_palette_group(const int address, const uint8_t value) {
		source_palette_[address & 0b0111] = ~value;

		mapped_palette_[base | 0b0000] = palette_entry<BitIndex{pair + 1, 0}, BitIndex{pair + 1, 4}, BitIndex{pair, 4}>();
		mapped_palette_[base | 0b0010] = palette_entry<BitIndex{pair + 1, 1}, BitIndex{pair + 1, 5}, BitIndex{pair, 5}>();
		mapped_palette_[base | 0b1000] = palette_entry<BitIndex{pair + 1, 2}, BitIndex{pair, 2}, BitIndex{pair, 6}>();
		mapped_palette_[base | 0b1010] = palette_entry<BitIndex{pair + 1, 3}, BitIndex{pair, 3}, BitIndex{pair, 7}>();
	}

	// User-selected base address; constrained to a 64-byte boundary by the setter.
	uint16_t screen_base_ = 0;

	// Parameters implied by mode selection.
	uint16_t mode_base_ = 0;
	bool mode_40_ = true;
	bool mode_text_ = false;
	enum class Bpp {
		One = 1, Two = 2, Four = 4
	} mode_bpp_ = Bpp::One;

	// Frame position.
	int v_count_ = 0;
	int h_count_ = 0;
	bool field_ = true;

	// Current working address.
	uint16_t row_addr_ = 0;		// Address, sans character row, adopted at the start of a row.
	uint16_t byte_addr_ = 0;	// Current working address, incremented as the raster moves across the line.
	int char_row_ = 0;			// Character row; 0–9 in text mode, 0–7 in graphics.

	// Sync states.
	bool vsync_int_ = false;	// True => vsync active.
	bool hsync_int_ = false;	// True => hsync active.

	// Horizontal timing parameters; all in terms of the 16Mhz pixel clock but conveniently all
	// divisible by 8, so it's safe to count time with a 2Mhz input.
	static constexpr int h_active = 640;
	static constexpr int hsync_start = 768;
	static constexpr int hsync_end = 832;
	static constexpr int h_reset_addr = 1016;
	static constexpr int h_total = 1024;	// Minor digression from the FPGA original here;
											// in this implementation the value is tested
											// _after_ position increment rather than before/instead.
											// So it needs to be one higher. Which is baked into
											// the constant to emphasise the all-divisible-by-8 property.

	static constexpr int h_half = h_total / 2;
	static constexpr int hburst_start = 856;
	static constexpr int hburst_end = 896;

	// Vertical timing parameters; all in terms of lines. As per the horizontal parameters above,
	// lines begin with their first visible pixel (or the equivalent position).
	static constexpr int v_active_gph = 256;
	static constexpr int v_active_txt = 250;
	static constexpr int v_disp_gph = v_active_gph - 1;
	static constexpr int v_disp_txt = v_active_txt - 1;
	static constexpr int vsync_start = 274;
	static constexpr int vsync_end = 276;
	static constexpr int v_rtc = 99;

	// Various signals that it was convenient to factor out.
	int v_total() const {
		return field_ ? 312 : 311;
	}

	bool last_line() const {
		return char_row_ == (mode_text_ ? 9 : 7);
	}

	bool in_blank() const {
		return
			h_count_ >= h_active ||
			(mode_text_ && v_count_ >= v_active_txt) ||
			(!mode_text_ && v_count_ >= v_active_gph) ||
			char_row_ >= 8;
	}

	bool is_v_end() const {
		return v_count_ == v_total();
	}

	uint8_t perform(int h_count, int v_count);
};
}
