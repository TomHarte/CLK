//
//  9918Base.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/12/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef TMS9918Base_hpp
#define TMS9918Base_hpp

#include "ClockConverter.hpp"

#include "../../../ClockReceiver/ClockReceiver.hpp"
#include "../../../Numeric/BitReverse.hpp"
#include "../../../Outputs/CRT/CRT.hpp"

#include "PersonalityTraits.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

namespace TI {
namespace TMS {

// The screen mode is a necessary predecessor to picking the line mode,
// which is the thing latched per line.
enum class ScreenMode {
	Blank,
	Text,
	MultiColour,
	ColouredText,
	Graphics,
	SMSMode4
};

enum class LineMode {
	Text,
	Character,
	Refresh,
	SMS
};

enum class MemoryAccess {
	Read, Write, None
};

// Temporary buffers collect a representation of each line prior to pixel serialisation.
struct LineBuffer {
	// The line mode describes the proper timing diagram for this line.
	LineMode line_mode = LineMode::Text;

	// Holds the horizontal scroll position to apply to this line;
	// of those VDPs currently implemented, affects the Master System only.
	uint8_t latched_horizontal_scroll = 0;

	// The names array holds pattern names, as an offset into memory, and
	// potentially flags also.
	struct {
		size_t offset = 0;
		uint8_t flags = 0;
	} names[40];

	// The patterns array holds tile patterns, corresponding 1:1 with names.
	// Four bytes per pattern is the maximum required by any
	// currently-implemented VDP.
	uint8_t patterns[40][4];

	/*
		Horizontal layout (on a 342-cycle clock):

			15 cycles right border
			58 cycles blanking & sync
			13 cycles left border

			... i.e. to cycle 86, then:

			border up to first_pixel_output_column;
			pixels up to next_border_column;
			border up to the end.

		e.g. standard 256-pixel modes will want to set
		first_pixel_output_column = 86, next_border_column = 342.
	*/
	int first_pixel_output_column = 94;
	int next_border_column = 334;
	size_t pixel_count = 256;

	// An active sprite is one that has been selected for composition onto
	// this line.
	struct ActiveSprite {
		int index = 0;		// The original in-table index of this sprite.
		int row = 0;		// The row of the sprite that should be drawn.
		int x = 0;			// The sprite's x position on screen.

		uint8_t image[4];		// Up to four bytes of image information.
		int shift_position = 0;	// An offset representing how much of the image information has already been drawn.
	} active_sprites[8];

	int active_sprite_slot = 0;		// A pointer to the slot into which a new active sprite will be deposited, if required.
	bool sprites_stopped = false;	// A special TMS feature is that a sentinel value can be used to prevent any further sprites
									// being evaluated for display. This flag determines whether the sentinel has yet been reached.

	void reset_sprite_collection();
};

struct LineBufferPointer {
	int row, column;
};

constexpr uint8_t StatusInterrupt = 0x80;
constexpr uint8_t StatusSpriteOverflow = 0x40;

constexpr int StatusSpriteCollisionShift = 5;
constexpr uint8_t StatusSpriteCollision = 0x20;

/// A container for personality-specific storage; see specific instances below.
template <Personality personality, typename Enable = void> struct Storage {
};

template <> struct Storage<Personality::TMS9918A> {
};

// Yamaha-specific storage.
template <Personality personality> struct Storage<personality, std::enable_if_t<is_yamaha_vdp(personality)>> {
	int selected_status_ = 0;

	int indirect_register_ = 0;
	bool increment_indirect_register_ = false;
};

// Master System-specific storage.
template <Personality personality> struct Storage<personality, std::enable_if_t<is_sega_vdp(personality)>> {
	// The SMS VDP has a programmer-set colour palette, with a dedicated patch of RAM. But the RAM is only exactly
	// fast enough for the pixel clock. So when the programmer writes to it, that causes a one-pixel glitch; there
	// isn't the bandwidth for the read both write to occur simultaneously. The following buffer therefore keeps
	// track of pending collisions, for visual reproduction.
	struct CRAMDot {
		LineBufferPointer location;
		uint32_t value;
	};
	std::vector<CRAMDot> upcoming_cram_dots_;

	// The Master System's additional colour RAM.
	uint32_t colour_ram_[32];
	bool cram_is_selected_ = false;

	// Fields below affect only the Master System output mode.

	// Programmer-set flags.
	bool vertical_scroll_lock_ = false;
	bool horizontal_scroll_lock_ = false;
	bool hide_left_column_ = false;
	bool shift_sprites_8px_left_ = false;
	bool mode4_enable_ = false;
	uint8_t horizontal_scroll_ = 0;
	uint8_t vertical_scroll_ = 0;

	// Holds the vertical scroll position for this frame; this is latched
	// once and cannot dynamically be changed until the next frame.
	uint8_t latched_vertical_scroll_ = 0;

	// Various resource addresses with VDP-version-specific modifications
	// built int.
	size_t pattern_name_address_;
	size_t sprite_attribute_table_address_;
	size_t sprite_generator_table_address_;
};

template <Personality personality> struct Base: public Storage<personality> {
	Base();

	static constexpr int output_lag = 11;	// i.e. pixel output will occur 11 cycles
											// after corresponding data read.

	static constexpr uint32_t palette_pack(uint8_t r, uint8_t g, uint8_t b) {
		#if TARGET_RT_BIG_ENDIAN
			return uint32_t((r << 24) | (g << 16) | (b << 8));
		#else
			return uint32_t((b << 16) | (g << 8) | r);
		#endif
	}

	// The default TMS palette.
	static constexpr std::array<uint32_t, 16> palette {
		palette_pack(0, 0, 0),
		palette_pack(0, 0, 0),
		palette_pack(33, 200, 66),
		palette_pack(94, 220, 120),

		palette_pack(84, 85, 237),
		palette_pack(125, 118, 252),
		palette_pack(212, 82, 77),
		palette_pack(66, 235, 245),

		palette_pack(252, 85, 84),
		palette_pack(255, 121, 120),
		palette_pack(212, 193, 84),
		palette_pack(230, 206, 128),

		palette_pack(33, 176, 59),
		palette_pack(201, 91, 186),
		palette_pack(204, 204, 204),
		palette_pack(255, 255, 255)
	};

	Outputs::CRT::CRT crt_;
	TVStandard tv_standard_ = TVStandard::NTSC;

	// Personality-specific metrics and converters.
	ClockConverter<personality> clock_converter_;

	// This VDP's DRAM.
	std::array<uint8_t, memory_size(personality)> ram_;

	// State of the DRAM/CRAM-access mechanism.
	uint16_t ram_pointer_ = 0;
	uint8_t read_ahead_buffer_ = 0;
	MemoryAccess queued_access_ = MemoryAccess::None;
	int cycles_until_access_ = 0;
	int minimum_access_column_ = 0;

	// The main status register.
	uint8_t status_ = 0;

	// Current state of programmer input.
	bool write_phase_ = false;	// Determines whether the VDP is expecting the low or high byte of a write.
	uint8_t low_write_ = 0;		// Buffers the low byte of a write.

	// Various programmable flags.
	bool mode1_enable_ = false;
	bool mode2_enable_ = false;
	bool mode3_enable_ = false;
	bool blank_display_ = false;
	bool sprites_16x16_ = false;
	bool sprites_magnified_ = false;
	bool generate_interrupts_ = false;
	int sprite_height_ = 8;

	// Programmer-specified addresses.
	size_t pattern_name_address_ = 0;				// i.e. address of the tile map.
	size_t colour_table_address_ = 0;				// address of the colour map (if applicable).
	size_t pattern_generator_table_address_ = 0;	// address of the tile contents.
	size_t sprite_attribute_table_address_ = 0;		// address of the sprite list.
	size_t sprite_generator_table_address_ = 0;		// address of the sprite contents.

	// Default colours.
	uint8_t text_colour_ = 0;
	uint8_t background_colour_ = 0;

	// Internal mechanisms for position tracking.
	int latched_column_ = 0;

	// A struct to contain timing information that is a function of the current mode.
	struct {
		/*
			Vertical layout:

			Lines 0 to [pixel_lines]: standard data fetch and drawing will occur.
			... to [first_vsync_line]: refresh fetches will occur and border will be output.
			.. to [2.5 or 3 lines later]: vertical sync is output.
			... to [total lines - 1]: refresh fetches will occur and border will be output.
			... for one line: standard data fetch will occur, without drawing.
		*/
		int total_lines = 262;
		int pixel_lines = 192;
		int first_vsync_line = 227;

		// Maximum number of sprite slots to populate;
		// if sprites beyond this number should be visible
		// then the appropriate status information will be set.
		int maximum_visible_sprites = 4;

		// Set the position, in cycles, of the two interrupts,
		// within a line.
		struct {
			int column = 4;
			int row = 193;
		} end_of_frame_interrupt_position;
		int line_interrupt_position = -1;

		// Enables or disabled the recognition of the sprite
		// list terminator, and sets the terminator value.
		bool allow_sprite_terminator = true;
		uint8_t sprite_terminator = 0xd0;
	} mode_timing_;

	uint8_t line_interrupt_target_ = 0xff;
	uint8_t line_interrupt_counter_ = 0;
	bool enable_line_interrupts_ = false;
	bool line_interrupt_pending_ = false;

	ScreenMode screen_mode_;
	LineBuffer line_buffers_[313];
	void posit_sprite(LineBuffer &buffer, int sprite_number, int sprite_y, int screen_row);

	// There is a delay between reading into the line buffer and outputting from there to the screen. That delay
	// is observeable because reading time affects availability of memory accesses and therefore time in which
	// to update sprites and tiles, but writing time affects when the palette is used and when the collision flag
	// may end up being set. So the two processes are slightly decoupled. The end of reading one line may overlap
	// with the beginning of writing the next, hence the two separate line buffers.
	LineBufferPointer output_pointer_, fetch_pointer_;

	int fetch_line() const;
	bool is_vertical_blank() const;
	bool is_horizontal_blank() const;

	int masked_address(int address) const;
	void write_vram(uint8_t);
	void write_register(uint8_t);
	void write_palette(uint8_t);
	void write_register_indirect(uint8_t);
	uint8_t read_vram();
	uint8_t read_register();
	uint8_t read_palette();
	uint8_t read_register_indirect();

	ScreenMode current_screen_mode() const {
		if(blank_display_) {
			return ScreenMode::Blank;
		}

		if constexpr (is_sega_vdp(personality)) {
			if(Storage<personality>::mode4_enable_) {
				return ScreenMode::SMSMode4;
			}
		}

		if(!mode1_enable_ && !mode2_enable_ && !mode3_enable_) {
			return ScreenMode::ColouredText;
		}

		if(mode1_enable_ && !mode2_enable_ && !mode3_enable_) {
			return ScreenMode::Text;
		}

		if(!mode1_enable_ && mode2_enable_ && !mode3_enable_) {
			return ScreenMode::Graphics;
		}

		if(!mode1_enable_ && !mode2_enable_ && mode3_enable_) {
			return ScreenMode::MultiColour;
		}

		// TODO: undocumented TMS modes.
		return ScreenMode::Blank;
	}

	void do_external_slot(int access_column) {
		// Don't do anything if the required time for the access to become executable
		// has yet to pass.
		if(access_column < minimum_access_column_) {
			return;
		}

		switch(queued_access_) {
			default: return;

			case MemoryAccess::Write:
				if constexpr (is_sega_vdp(personality)) {
					if(Storage<personality>::cram_is_selected_) {
						// Adjust the palette. In a Master System blue has a slightly different
						// scale; cf. https://www.retrorgb.com/sega-master-system-non-linear-blue-channel-findings.html
						constexpr uint8_t rg_scale[] = {0, 85, 170, 255};
						constexpr uint8_t b_scale[] = {0, 104, 170, 255};
						Storage<personality>::colour_ram_[ram_pointer_ & 0x1f] = palette_pack(
							rg_scale[(read_ahead_buffer_ >> 0) & 3],
							rg_scale[(read_ahead_buffer_ >> 2) & 3],
							b_scale[(read_ahead_buffer_ >> 4) & 3]
						);

						// Schedule a CRAM dot; this is scheduled for wherever it should appear
						// on screen. So it's wherever the output stream would be now. Which
						// is output_lag cycles ago from the point of view of the input stream.
						auto &dot = Storage<personality>::upcoming_cram_dots_.emplace_back();
						dot.location.column = fetch_pointer_.column - output_lag;
						dot.location.row = fetch_pointer_.row;

						// Handle before this row conditionally; then handle after (or, more realistically,
						// exactly at the end of) naturally.
						if(dot.location.column < 0) {
							--dot.location.row;
							dot.location.column += 342;
						}
						dot.location.row += dot.location.column / 342;
						dot.location.column %= 342;

						dot.value = Storage<personality>::colour_ram_[ram_pointer_ & 0x1f];
						break;
					}
				}
				ram_[ram_pointer_ & memory_mask(personality)] = read_ahead_buffer_;
			break;
			case MemoryAccess::Read:
				read_ahead_buffer_ = ram_[ram_pointer_ & memory_mask(personality)];
			break;
		}
		++ram_pointer_;
		queued_access_ = MemoryAccess::None;
	}

	// Various fetchers.
	template<bool use_end> void fetch_tms_refresh(int start, int end);
	template<bool use_end> void fetch_tms_text(int start, int end);
	template<bool use_end> void fetch_tms_character(int start, int end);

	template<bool use_end> void fetch_yamaha_refresh(int start, int end);
	template<bool use_end> void fetch_yamaha_no_sprites(int start, int end);
	template<bool use_end> void fetch_yamaha_sprites(int start, int end);

	template<bool use_end> void fetch_sms(int start, int end);

	// A helper function to output the current border colour for
	// the number of cycles supplied.
	void output_border(int cycles, uint32_t cram_dot);

	// Output serialisation state.
	uint32_t *pixel_target_ = nullptr, *pixel_origin_ = nullptr;
	bool asked_for_write_area_ = false;

	// Output serialisers.
	void draw_tms_character(int start, int end);
	void draw_tms_text(int start, int end);
	void draw_sms(int start, int end, uint32_t cram_dot);
};

#include "Fetch.hpp"
#include "Draw.hpp"

}
}

#endif /* TMS9918Base_hpp */
