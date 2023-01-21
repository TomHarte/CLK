//
//  9918.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/11/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "../9918.hpp"

#include <cassert>
#include <cstring>
#include <cstdlib>
#include "../../../Outputs/Log.hpp"

using namespace TI::TMS;

namespace {

// 342 internal cycles are 228/227.5ths of a line, so 341.25 cycles should be a whole
// line. Therefore multiply everything by four, but set line length to 1365 rather than 342*4 = 1368.
constexpr unsigned int CRTCyclesPerLine = 1365;
constexpr unsigned int CRTCyclesDivider = 4;

}

template <Personality personality>
Base<personality>::Base() :
	crt_(CRTCyclesPerLine, CRTCyclesDivider, Outputs::Display::Type::NTSC60, Outputs::Display::InputDataType::Red8Green8Blue8) {
	// Unimaginatively, this class just passes RGB through to the shader. Investigation is needed
	// into whether there's a more natural form. It feels unlikely given the diversity of chips modelled.

	if constexpr (is_sega_vdp(personality)) {
		mode_timing_.line_interrupt_position = 64;

		mode_timing_.end_of_frame_interrupt_position.column = 63;
		mode_timing_.end_of_frame_interrupt_position.row = 193;
	}

	// Establish that output is delayed after reading by `output_lag` cycles; start
	// at a random position.
	fetch_pointer_.row = rand() % 262;
	fetch_pointer_.column = rand() % (Timing<personality>::CyclesPerLine - output_lag);
	output_pointer_.row = output_pointer_.row;
	output_pointer_.column = output_pointer_.column + output_lag;
}

template <Personality personality>
TMS9918<personality>::TMS9918() {
	this->crt_.set_display_type(Outputs::Display::DisplayType::RGB);
	this->crt_.set_visible_area(Outputs::Display::Rect(0.07f, 0.0375f, 0.875f, 0.875f));

	// The TMS remains in-phase with the NTSC colour clock; this is an empirical measurement
	// intended to produce the correct relationship between the hard edges between pixels and
	// the colour clock. It was eyeballed rather than derived from any knowledge of the TMS
	// colour burst generator because I've yet to find any.
	this->crt_.set_immediate_default_phase(0.85f);
}

template <Personality personality>
void TMS9918<personality>::set_tv_standard(TVStandard standard) {
	this->tv_standard_ = standard;
	switch(standard) {
		case TVStandard::PAL:
			this->mode_timing_.total_lines = 313;
			this->mode_timing_.first_vsync_line = 253;
			this->crt_.set_new_display_type(CRTCyclesPerLine, Outputs::Display::Type::PAL50);
		break;
		default:
			this->mode_timing_.total_lines = 262;
			this->mode_timing_.first_vsync_line = 227;
			this->crt_.set_new_display_type(CRTCyclesPerLine, Outputs::Display::Type::NTSC60);
		break;
	}
}

template <Personality personality>
void TMS9918<personality>::set_scan_target(Outputs::Display::ScanTarget *scan_target) {
	this->crt_.set_scan_target(scan_target);
}

template <Personality personality>
Outputs::Display::ScanStatus TMS9918<personality>::get_scaled_scan_status() const {
	// The input was scaled by 3/4 to convert half cycles to internal ticks,
	// so undo that and also allow for: (i) the multiply by 4 that it takes
	// to reach the CRT; and (ii) the fact that the half-cycles value was scaled,
	// and this should really reply in whole cycles.
	return this->crt_.get_scaled_scan_status() * (4.0f / (3.0f * 8.0f));
}

template <Personality personality>
void TMS9918<personality>::set_display_type(Outputs::Display::DisplayType display_type) {
	this->crt_.set_display_type(display_type);
}

template <Personality personality>
Outputs::Display::DisplayType TMS9918<personality>::get_display_type() const {
	return this->crt_.get_display_type();
}

void LineBuffer::reset_sprite_collection() {
	sprites_stopped = false;
	active_sprite_slot = 0;

	for(int c = 0; c < 8; ++c) {
		active_sprites[c].shift_position = 0;
	}
}

template <Personality personality>
void Base<personality>::posit_sprite(LineBuffer &buffer, int sprite_number, int sprite_position, int screen_row) {
	if(!(status_ & StatusSpriteOverflow)) {
		status_ = uint8_t((status_ & ~0x1f) | (sprite_number & 0x1f));
	}
	if(buffer.sprites_stopped) return;

	// A sprite Y of 208 means "don't scan the list any further".
	if(mode_timing_.allow_sprite_terminator && sprite_position == mode_timing_.sprite_terminator) {
		buffer.sprites_stopped = true;
		return;
	}

	const int sprite_row = (((screen_row + 1) % mode_timing_.total_lines) - ((sprite_position + 1) & 255)) & 255;
	if(sprite_row < 0 || sprite_row >= sprite_height_) return;

	if(buffer.active_sprite_slot == mode_timing_.maximum_visible_sprites) {
		status_ |= StatusSpriteOverflow;
		return;
	}

	LineBuffer::ActiveSprite &sprite = buffer.active_sprites[buffer.active_sprite_slot];
	sprite.index = sprite_number;
	sprite.row = sprite_row >> (sprites_magnified_ ? 1 : 0);
	++buffer.active_sprite_slot;
}

template <Personality personality>
void TMS9918<personality>::run_for(const HalfCycles cycles) {
	// As specific as I've been able to get:
	// Scanline time is always 228 cycles.
	// PAL output is 313 lines total. NTSC output is 262 lines total.
	// Interrupt is signalled upon entering the lower border.

	// Convert 456 clocked half cycles per line to 342 internal cycles per line;
	// the internal clock is 1.5 times the nominal 3.579545 Mhz that I've advertised
	// for this part. So multiply by three quarters.
	const int int_cycles = this->clock_converter_.to_internal(cycles.as<int>());
	if(!int_cycles) return;

	// There are two intertwined processes here, 'writing' (which means writing to the
	// line buffers, i.e. it's everything to do with collecting a line) and 'reading'
	// (which means reading from the line buffers and generating video).
	int write_cycles_pool = int_cycles;
	int read_cycles_pool = int_cycles;

	while(write_cycles_pool || read_cycles_pool) {
#ifndef NDEBUG
		LineBufferPointer backup = this->output_pointer_;
#endif

		if(write_cycles_pool) {
			// Determine how much writing to do.
			const int write_cycles = std::min(
				Timing<personality>::CyclesPerLine - this->fetch_pointer_.column,
				write_cycles_pool
			);
			const int end_column = this->fetch_pointer_.column + write_cycles;
			LineBuffer &line_buffer = this->line_buffers_[this->fetch_pointer_.row];

			// Determine what this does to any enqueued VRAM access.
			this->minimum_access_column_ = this->fetch_pointer_.column + this->cycles_until_access_;
			this->cycles_until_access_ -= write_cycles;


			// ---------------------------------------
			// Latch scrolling position, if necessary.
			// ---------------------------------------
			if constexpr (is_sega_vdp(personality)) {
				if(this->fetch_pointer_.column < 61 && end_column >= 61) {
					if(!this->fetch_pointer_.row) {
						Storage<personality>::latched_vertical_scroll_ = Storage<personality>::vertical_scroll_;

						if(Storage<personality>::mode4_enable_) {
							this->mode_timing_.pixel_lines = 192;
							if(this->mode2_enable_ && this->mode1_enable_) this->mode_timing_.pixel_lines = 224;
							if(this->mode2_enable_ && this->mode3_enable_) this->mode_timing_.pixel_lines = 240;

							this->mode_timing_.allow_sprite_terminator = this->mode_timing_.pixel_lines == 192;
							this->mode_timing_.first_vsync_line = (this->mode_timing_.total_lines + this->mode_timing_.pixel_lines) >> 1;

							this->mode_timing_.end_of_frame_interrupt_position.row = this->mode_timing_.pixel_lines + 1;
						}
					}
					line_buffer.latched_horizontal_scroll = Storage<personality>::horizontal_scroll_;
				}
			}



			// ------------------------
			// Perform memory accesses.
			// ------------------------
#define fetch(function, clock)																\
	const int first_window = from_internal<personality, clock>(this->fetch_pointer_.column);\
	const int final_window = from_internal<personality, clock>(end_column);					\
	if(first_window == final_window) break;													\
	if(final_window != clock_rate<personality, clock>()) {									\
		function<true>(first_window, final_window);											\
	} else {																				\
		function<false>(first_window, final_window);										\
	}

			switch(line_buffer.line_mode) {
				case LineMode::Text:		{	fetch(this->template fetch_tms_text, Clock::TMSMemoryWindow);		}	break;
				case LineMode::Character:	{	fetch(this->template fetch_tms_character, Clock::TMSMemoryWindow);	}	break;
				case LineMode::SMS:			{	fetch(this->template fetch_sms, Clock::TMSMemoryWindow);			}	break;
				case LineMode::Refresh:		{	fetch(this->template fetch_tms_refresh, Clock::TMSMemoryWindow);	}	break;
			}

#undef fetch



			// -------------------------------
			// Check for interrupt conditions.
			// -------------------------------
			if(this->fetch_pointer_.column < this->mode_timing_.line_interrupt_position && end_column >= this->mode_timing_.line_interrupt_position) {
				// The Sega VDP offers a decrementing counter for triggering line interrupts;
				// it is reloaded either when it overflows or upon every non-pixel line after the first.
				// It is otherwise decremented.
				if constexpr (is_sega_vdp(personality)) {
					if(this->fetch_pointer_.row >= 0 && this->fetch_pointer_.row <= this->mode_timing_.pixel_lines) {
						--this->line_interrupt_counter_;
						if(this->line_interrupt_counter_ == 0xff) {
							this->line_interrupt_pending_ = true;
							this->line_interrupt_counter_ = this->line_interrupt_target_;
						}
					} else {
						this->line_interrupt_counter_ = this->line_interrupt_target_;
					}
				}

				// TODO: the V9938 provides line interrupts from direct specification of the target line.
				// So life is easy.
			}

			if(
				this->fetch_pointer_.row == this->mode_timing_.end_of_frame_interrupt_position.row &&
				this->fetch_pointer_.column < this->mode_timing_.end_of_frame_interrupt_position.column &&
				end_column >= this->mode_timing_.end_of_frame_interrupt_position.column
			) {
				this->status_ |= StatusInterrupt;
			}



			// -------------
			// Advance time.
			// -------------
			this->fetch_pointer_.column = end_column;
			write_cycles_pool -= write_cycles;

			if(this->fetch_pointer_.column == Timing<personality>::CyclesPerLine) {
				this->fetch_pointer_.column = 0;
				this->fetch_pointer_.row = (this->fetch_pointer_.row + 1) % this->mode_timing_.total_lines;
				LineBuffer &next_line_buffer = this->line_buffers_[this->fetch_pointer_.row];

				// Establish the current screen output mode, which will be captured as a
				// line mode momentarily.
				this->screen_mode_ = this->current_screen_mode();

				// Based on the output mode, pick a line mode.
				next_line_buffer.first_pixel_output_column = Timing<personality>::FirstPixelCycle;
				next_line_buffer.next_border_column = Timing<personality>::CyclesPerLine;
				next_line_buffer.pixel_count = 256;
				this->mode_timing_.maximum_visible_sprites = 4;
				switch(this->screen_mode_) {
					case ScreenMode::Text:
						next_line_buffer.line_mode = LineMode::Text;
						next_line_buffer.first_pixel_output_column = Timing<personality>::FirstTextCycle;
						next_line_buffer.next_border_column = Timing<personality>::LastTextCycle;
						next_line_buffer.pixel_count = 240;
					break;
					case ScreenMode::SMSMode4:
						next_line_buffer.line_mode = LineMode::SMS;
						this->mode_timing_.maximum_visible_sprites = 8;
					break;
					default:
						next_line_buffer.line_mode = LineMode::Character;
					break;
				}

				if(
					(this->screen_mode_ == ScreenMode::Blank) ||
					this->is_vertical_blank())
						next_line_buffer.line_mode = LineMode::Refresh;
			}
		}


#ifndef NDEBUG
		assert(backup.row == this->output_pointer_.row && backup.column == this->output_pointer_.column);
		backup = this->fetch_pointer_;
#endif


		if(read_cycles_pool) {
			// Determine how much time has passed in the remainder of this line, and proceed.
			const int target_read_cycles = std::min(
				Timing<personality>::CyclesPerLine - this->output_pointer_.column,
				read_cycles_pool
			);
			int read_cycles_performed = 0;
			uint32_t next_cram_value = 0;

			while(read_cycles_performed < target_read_cycles) {
				int read_cycles = target_read_cycles - read_cycles_performed;
				if(!read_cycles) continue;

				// Grab the next CRAM dot value and schedule a break in output if applicable.
				const uint32_t cram_value = next_cram_value;
				if constexpr (is_sega_vdp(personality)) {
					next_cram_value = 0;

					if(!this->upcoming_cram_dots_.empty() && this->upcoming_cram_dots_.front().location.row == this->output_pointer_.row) {
						int time_until_dot = this->upcoming_cram_dots_.front().location.column - this->output_pointer_.column;

						if(time_until_dot < read_cycles) {
							read_cycles = time_until_dot;
							next_cram_value = this->upcoming_cram_dots_.front().value;
							this->upcoming_cram_dots_.erase(this->upcoming_cram_dots_.begin());
						}
					}
				}

				read_cycles_performed += read_cycles;

				const int end_column = this->output_pointer_.column + read_cycles;
				LineBuffer &line_buffer = this->line_buffers_[this->output_pointer_.row];


				// --------------------
				// Output video stream.
				// --------------------

#define crt_convert(action, time)		this->crt_.action(from_internal<personality, Clock::CRT>(time))
#define output_sync(x)					crt_convert(output_sync, x)
#define output_blank(x)					crt_convert(output_blank, x)
#define output_default_colour_burst(x)	crt_convert(output_default_colour_burst, x)

#define intersect(left, right, code)	{	\
		const int start = std::max(this->output_pointer_.column, left);	\
		const int end = std::min(end_column, right);	\
		if(end > start) {\
			code;\
		}\
	}

#define border(left, right)	intersect(left, right, this->output_border(end - start, cram_value))

				if(line_buffer.line_mode == LineMode::Refresh || this->output_pointer_.row > this->mode_timing_.pixel_lines) {
					if(
						this->output_pointer_.row >= this->mode_timing_.first_vsync_line &&
						this->output_pointer_.row < this->mode_timing_.first_vsync_line + 4
					) {
						// Vertical sync.
						// TODO: the Mega Drive supports interlaced video, I think?
						if(end_column == Timing<personality>::CyclesPerLine) {
							output_sync(Timing<personality>::CyclesPerLine);
						}
					} else {
						// Right border.
						border(0, Timing<personality>::EndOfRightBorder);

						// Blanking region: output the entire sequence when the cursor
						// crosses the start-of-border point.
						if(
							this->output_pointer_.column < Timing<personality>::StartOfLeftBorder &&
							end_column >= Timing<personality>::StartOfLeftBorder
						) {
							output_blank(Timing<personality>::StartOfSync - Timing<personality>::EndOfRightBorder);
							output_sync(Timing<personality>::EndOfSync - Timing<personality>::StartOfSync);
							output_blank(Timing<personality>::StartOfColourBurst - Timing<personality>::EndOfSync);
							output_default_colour_burst(Timing<personality>::EndOfColourBurst - Timing<personality>::StartOfColourBurst);
							output_blank(Timing<personality>::StartOfLeftBorder - Timing<personality>::EndOfColourBurst);
						}

						// Border colour for the rest of the line.
						border(Timing<personality>::StartOfLeftBorder, Timing<personality>::CyclesPerLine);
					}
				} else {
					// Right border.
					border(0, Timing<personality>::EndOfRightBorder);

					// Blanking region.
					if(
						this->output_pointer_.column < Timing<personality>::StartOfLeftBorder &&
						end_column >= Timing<personality>::StartOfLeftBorder
					) {
						output_blank(Timing<personality>::StartOfSync - Timing<personality>::EndOfRightBorder);
						output_sync(Timing<personality>::EndOfSync - Timing<personality>::StartOfSync);
						output_blank(Timing<personality>::StartOfColourBurst - Timing<personality>::EndOfSync);
						output_default_colour_burst(Timing<personality>::EndOfColourBurst - Timing<personality>::StartOfColourBurst);
						output_blank(Timing<personality>::StartOfLeftBorder - Timing<personality>::EndOfColourBurst);
					}

					// Left border.
					border(Timing<personality>::StartOfLeftBorder, line_buffer.first_pixel_output_column);

#define draw(function, clock) {																				\
	const int relative_start = from_internal<personality, clock>(start - line_buffer.first_pixel_output_column);	\
	const int relative_end = from_internal<personality, clock>(end - line_buffer.first_pixel_output_column);		\
	if(relative_start == relative_end) break;																	\
	this->function; }

					// Pixel region.
					intersect(
						line_buffer.first_pixel_output_column,
						line_buffer.next_border_column,
						if(!this->asked_for_write_area_) {
							this->asked_for_write_area_ = true;

							this->pixel_origin_ = this->pixel_target_ = reinterpret_cast<uint32_t *>(
								this->crt_.begin_data(line_buffer.pixel_count)
							);
						}

						if(this->pixel_target_) {
							switch(line_buffer.line_mode) {
								case LineMode::SMS:			draw(draw_sms(relative_start, relative_end, cram_value), Clock::TMSPixel);	break;
								case LineMode::Character:	draw(draw_tms_character(relative_start, relative_end), Clock::TMSPixel);	break;
								case LineMode::Text:		draw(draw_tms_text(relative_start, relative_end), Clock::TMSPixel);			break;

								case LineMode::Refresh:		break;	/* Dealt with elsewhere. */
							}
						}

						if(end == line_buffer.next_border_column) {
							const int length = line_buffer.next_border_column - line_buffer.first_pixel_output_column;
							this->crt_.output_data(from_internal<personality, Clock::CRT>(length), line_buffer.pixel_count);
							this->pixel_origin_ = this->pixel_target_ = nullptr;
							this->asked_for_write_area_ = false;
						}
					);

#undef draw

					// Additional right border, if called for.
					if(line_buffer.next_border_column != Timing<personality>::CyclesPerLine) {
						border(line_buffer.next_border_column, Timing<personality>::CyclesPerLine);
					}
				}

#undef border
#undef intersect

#undef crt_convert
#undef output_sync
#undef output_blank
#undef output_default_colour_burst



				// -------------
				// Advance time.
				// -------------
				this->output_pointer_.column = end_column;
			}

			read_cycles_pool -= target_read_cycles;
			if(this->output_pointer_.column == Timing<personality>::CyclesPerLine) {
				this->output_pointer_.column = 0;
				this->output_pointer_.row = (this->output_pointer_.row + 1) % this->mode_timing_.total_lines;
			}
		}

		assert(backup.row == this->fetch_pointer_.row && backup.column == this->fetch_pointer_.column);
	}
}

template <Personality personality>
void Base<personality>::output_border(int cycles, [[maybe_unused]] uint32_t cram_dot) {
	cycles = from_internal<personality, Clock::CRT>(cycles);

	uint32_t border_colour;
	if constexpr (is_sega_vdp(personality)) {
		border_colour = Storage<personality>::colour_ram_[16 + background_colour_];

		if(cram_dot) {
			uint32_t *const pixel_target = reinterpret_cast<uint32_t *>(crt_.begin_data(1));
			if(pixel_target) {
				*pixel_target = border_colour | cram_dot;
			}

			// Four CRT cycles is one pixel width, so this doesn't need clock conversion.
			// TODO: on the Mega Drive it may be only 3 colour cycles, depending on mode.
			crt_.output_level(4);
			cycles -= 4;
		}
	} else {
		border_colour = palette[background_colour_];
	}

	if(!cycles) {
		return;
	}

	// If the border colour is 0, that can be communicated
	// more efficiently as an explicit blank.
	if(border_colour) {
		uint32_t *const pixel_target = reinterpret_cast<uint32_t *>(crt_.begin_data(1));
		if(pixel_target) {
			*pixel_target = border_colour;
		}
		crt_.output_level(cycles);
	} else {
		crt_.output_blank(cycles);
	}
}

// MARK: - External interface.

template <Personality personality>
int Base<personality>::masked_address(int address) const {
	if constexpr (is_yamaha_vdp(personality)) {
		return address & 3;
	} else {
		return address & 1;
	}
}

template <Personality personality>
void Base<personality>::write_vram(uint8_t value) {
	// Latch  the value and exit.
	write_phase_ = false;

	// Enqueue the write to occur at the next available slot.
	read_ahead_buffer_ = value;
	queued_access_ = MemoryAccess::Write;
	cycles_until_access_ = Timing<personality>::VRAMAccessDelay;
}

template <Personality personality>
void Base<personality>::commit_register(int reg, uint8_t value) {
	if constexpr (is_yamaha_vdp(personality)) {
		reg &= 0x3f;
	} else if constexpr (is_sega_vdp(personality)) {
		if(reg & 0x40) {
			Storage<personality>::cram_is_selected_ = true;
			return;
		}
		reg &= 0xf;
	} else {
		reg &= 0x7;
	}

	//
	// Generic TMS functionality.
	//
	switch(reg) {
		case 0:
			mode2_enable_ = value & 0x02;
		break;

		case 1:
			blank_display_ = !(value & 0x40);
			generate_interrupts_ = value & 0x20;
			mode1_enable_ = value & 0x10;
			mode3_enable_ = value & 0x08;
			sprites_16x16_ = value & 0x02;
			sprites_magnified_ = value & 0x01;

			sprite_height_ = 8;
			if(sprites_16x16_) sprite_height_ <<= 1;
			if(sprites_magnified_) sprite_height_ <<= 1;
		break;

		case 2:
			pattern_name_address_ = size_t((value & 0xf) << 10) | 0x3ff;
		break;

		case 3:
			colour_table_address_ = size_t(value << 6) | 0x3f;
		break;

		case 4:
			pattern_generator_table_address_ = size_t((value & 0x07) << 11) | 0x7ff;
		break;

		case 5:
			sprite_attribute_table_address_ = size_t((value & 0x7f) << 7) | 0x7f;
		break;

		case 6:
			sprite_generator_table_address_ = size_t((value & 0x07) << 11) | 0x7ff;
		break;

		case 7:
			text_colour_ = value >> 4;
			background_colour_ = value & 0xf;
		break;

		default: break;
	}

	//
	// Sega extensions.
	//
	if constexpr (is_sega_vdp(personality)) {
		switch(reg) {
			default: break;

			case 0:
				Storage<personality>::vertical_scroll_lock_ = value & 0x80;
				Storage<personality>::horizontal_scroll_lock_ = value & 0x40;
				Storage<personality>::hide_left_column_ = value & 0x20;
				enable_line_interrupts_ = value & 0x10;
				Storage<personality>::shift_sprites_8px_left_ = value & 0x08;
				Storage<personality>::mode4_enable_ = value & 0x04;
			break;

			case 2:
				Storage<personality>::pattern_name_address_ = pattern_name_address_ | ((personality == TMS::SMSVDP) ? 0x000 : 0x400);
			break;

			case 5:
				Storage<personality>::sprite_attribute_table_address_ = sprite_attribute_table_address_ | ((personality == TMS::SMSVDP) ? 0x00 : 0x80);
			break;

			case 6:
				Storage<personality>::sprite_generator_table_address_ = sprite_generator_table_address_ | ((personality == TMS::SMSVDP) ? 0x0000 : 0x1800);
			break;

			case 8:
				Storage<personality>::horizontal_scroll_ = value;
			break;

			case 9:
				Storage<personality>::vertical_scroll_ = value;
			break;

			case 10:
				line_interrupt_target_ = value;
			break;
		}
	}

	//
	// Yamaha extensions.
	//
	if constexpr (is_yamaha_vdp(personality)) {
		switch(reg) {
			default: break;

			case 0:
				LOG("TODO: Yamaha additional mode selection; " << PADHEX(2) << +value);
				// b1–b3: M3–M5
				// b4: enable horizontal retrace interrupt
				// b5: enable light pen interrupts
				// b6: set colour bus to input or output mode
			break;

			case 8:
				LOG("TODO: Yamaha VRAM organisation, sprite disable, etc; " << PADHEX(2) << +value);
				// b7: "1 = input on colour bus, enable mouse; 1 = output on colour bus, disable mouse" [documentation clearly in error]
				// b6: 1 = enable light pen
				// b5: sets the colour of code 0 to the colour of the palette (???)
				// b4: 1 = colour bus in input mode; 0 = colour bus in output mode
				// b3: 1 = VRAM is 64kx1 or 64kx4; 0 = 16kx1 or 16kx4; affects refresh.
				// b1: 1 = disable sprites (and release sprite access slots)
				// b0: 1 = output in grayscale
			break;

			case 9:
				LOG("TODO: Yamaha line count, interlace, etc; " << PADHEX(2) << +value);
				// b7: 1 = 212 lines of pixels; 0 = 192
				// b5 & b4: select simultaneous mode (seems to relate to line length and in-phase colour?)
				// b3: 1 = interlace on
				// b2: 1 = display two graphic screens interchangeably by even/odd field
				// b1: 1 = PAL mode; 0 = NTSC mode
				// b0: 1 = [dot clock] DLCLK is input; 0 = DLCLK is output
			break;

			case 10:
				LOG("TODO: Yamaha colour table high bits; " << PADHEX(2) << +value);
				// b0–b2: A14–A16 of the colour table.
			break;

			case 11:
				LOG("TODO: Yamaha sprite table high bits; " << PADHEX(2) << +value);
				// b0–b1: A15–A16 of the sprite table.
			break;

			case 12:
				LOG("TODO: Yamaha text and background blink colour; " << PADHEX(2) << +value);
				// as per register 7, but in blink mode.
			break;

			case 13:
				LOG("TODO: Yamaha blink display times; " << PADHEX(2) << +value);
				// b0–b3: display time for odd page;
				// b4–b7: display time for even page.
			break;

			case 14:
				LOG("TODO: Yamaha A14–A16 selection; " << PADHEX(2) << +value);
				// b0–b2: A14–A16 of address counter (i.e. ram_pointer_)
			break;

			case 15:
				Storage<personality>::selected_status_ = value & 0xf;
			break;

			case 16:
				Storage<personality>::palette_entry_ = value;
				// b0–b3: palette entry for writing on port 2; autoincrements upon every write.
			break;

			case 17:
				Storage<personality>::increment_indirect_register_ = !(value & 0x80);
				Storage<personality>::indirect_register_ = value & 0x3f;
			break;

			case 18:
				LOG("TODO: Yamaha position adjustment; " << PADHEX(2) << +value);
				// b0-b3: horizontal adjustment
				// b4-b7: vertical adjustment
			break;

			case 19:
				LOG("TODO: Yamaha interrupt line; " << PADHEX(2) << +value);
				// b0–b7: line to match for interrupts (if eabled)
			break;

			case 20:
			case 21:
			case 22:
				LOG("TODO: Yamaha colour burst selection; " << PADHEX(2) << +value);
				// Documentation is "fill with 0s for no colour burst; magic pattern for colour burst"
			break;

			case 23:
				LOG("TODO: Yamaha vertical offset; " << PADHEX(2) << +value);
				// i.e. scrolling.
			break;

			case 32:
			case 33:
				LOG("TODO: Yamaha command source x; " << PADHEX(2) << +value);
			break;

			case 34:
			case 35:
				LOG("TODO: Yamaha command source y; " << PADHEX(2) << +value);
			break;

			case 36:
			case 37:
				LOG("TODO: Yamaha command destination x; " << PADHEX(2) << +value);
			break;

			case 38:
			case 39:
				LOG("TODO: Yamaha command destination y; " << PADHEX(2) << +value);
			break;

			case 40:
			case 41:
				LOG("TODO: Yamaha command size x; " << PADHEX(2) << +value);
			break;

			case 42:
			case 43:
				LOG("TODO: Yamaha command size y; " << PADHEX(2) << +value);
			break;

			case 44:
				LOG("TODO: Yamaha command colour; " << PADHEX(2) << +value);
			break;

			case 45:
				LOG("TODO: Yamaha VRAM bank selection addressing and command arguments; " << PADHEX(2) << +value);
				// b6: 0 = video RAM; 1 = expandion RAM.
				// b5: MXD (???)
				// b4: MXS
				// b3: DIY
				// b2: DIX
				// b1: EQ
				// b0: MAJ
			break;

			case 46:
				LOG("TODO: Yamaha command; " << PADHEX(2) << +value);
				// b0–b3: LO0–LO3 (???)
				// b4–b7: CM0-CM3 (???)
			break;
		}
	}
}

template <Personality personality>
void Base<personality>::write_register(uint8_t value) {
	// Writes to address 1 are performed in pairs; if this is the
	// low byte of a value, store it and wait for the high byte.
	if(!write_phase_) {
		low_write_ = value;
		write_phase_ = true;

		// The initial write should half update the access pointer.
		ram_pointer_ = (ram_pointer_ & 0xff00) | low_write_;
		return;
	}

	// The RAM pointer is always set on a second write, regardless of
	// whether the caller is intending to enqueue a VDP operation.
	ram_pointer_ = (ram_pointer_ & 0x00ff) | uint16_t(value << 8);

	write_phase_ = false;
	if(value & 0x80) {
		commit_register(value, low_write_);
	} else {
		// This is an access via the RAM pointer.
		if(!(value & 0x40)) {
			// A read request is enqueued upon setting the address; conversely a write
			// won't be enqueued unless and until some actual data is supplied.
			queued_access_ = MemoryAccess::Read;
			cycles_until_access_ = Timing<personality>::VRAMAccessDelay;
		}

		if constexpr (is_sega_vdp(personality)) {
			Storage<personality>::cram_is_selected_ = false;
		}
	}
}

template <Personality personality>
void Base<personality>::write_palette(uint8_t value) {
	if constexpr (is_yamaha_vdp(personality)) {
		if(!write_phase_) {
			Storage<personality>::new_colour_ = value;
			write_phase_ = true;
			return;
		}

		write_phase_ = false;

		const uint8_t r = ((Storage<personality>::new_colour_ >> 4) & 7) * 255 / 7;
		const uint8_t g = (value & 7) * 255 / 7;
		const uint8_t b = (Storage<personality>::new_colour_ & 7) * 255 / 7;

		Storage<personality>::palette_[Storage<personality>::palette_entry_ & 0xf] = palette_pack(r, g, b);
		++Storage<personality>::palette_entry_;
	}
}

template <Personality personality>
void Base<personality>::write_register_indirect([[maybe_unused]] uint8_t value) {
	if constexpr (is_yamaha_vdp(personality)) {
		// Register 17 cannot be written to indirectly.
		if(Storage<personality>::indirect_register_ != 17) {
			commit_register(Storage<personality>::indirect_register_, value);
		}
		Storage<personality>::indirect_register_ += Storage<personality>::increment_indirect_register_;
	}
}

template <Personality personality>
void TMS9918<personality>::write(int address, uint8_t value) {
	switch(this->masked_address(address)) {
		default: break;
		case 0:	this->write_vram(value);				break;
		case 1:	this->write_register(value);			break;
		case 2:	this->write_palette(value);				break;
		case 3: this->write_register_indirect(value);	break;
	}
}

template <Personality personality>
uint8_t Base<personality>::read_vram() {
	// Take whatever is currently in the read-ahead buffer and
	// enqueue a further read to occur at the next available slot.
	const uint8_t result = read_ahead_buffer_;
	queued_access_ = MemoryAccess::Read;
	return result;
}

template <Personality personality>
uint8_t Base<personality>::read_register() {
	if constexpr (is_yamaha_vdp(personality)) {
		switch(Storage<personality>::selected_status_) {
			case 0: break;

			case 2:
				// b7 = transfer ready flag (i.e. VDP ready for next transfer)
				// b6 = 1 during vblank
				// b5 = 1 during hblank
				// b4 = set if colour detected during search command
				// b1 = display field odd/even
				// b0 = command ongoing
				return
					(queued_access_ == MemoryAccess::None ? 0x80 : 0x00) |
					(is_vertical_blank() ? 0x40 : 0x00) |
					(is_horizontal_blank() ? 0x20 : 0x00);

			break;
		}
	}

	// Gets the status register.
	const uint8_t result = status_;
	status_ &= ~(StatusInterrupt | StatusSpriteOverflow | StatusSpriteCollision);
	line_interrupt_pending_ = false;
	return result;
}

template <Personality personality>
uint8_t Base<personality>::read_palette() {
	LOG("Palette read TODO");
	return 0xff;
}

template <Personality personality>
uint8_t Base<personality>::read_register_indirect() {
	LOG("Register indirect read TODO");
	return 0xff;
}

template <Personality personality>
uint8_t TMS9918<personality>::read(int address) {
	// TODO: is this still a global effect of reads, even in the world of the Yamahas?
	this->write_phase_ = false;

	switch(this->masked_address(address)) {
		default: return 0xff;
		case 0:	return this->read_vram();
		case 1:	return this->read_register();
		case 2:	return this->read_palette();
		case 3: return this->read_register_indirect();
	}
}

// MARK: - Ephemeral state.

template <Personality personality>
int Base<personality>::fetch_line() const {
	// This is the proper Master System value; TODO: what's correct for Yamaha, etc?
	constexpr int row_change_position = 63;

	return
		(this->fetch_pointer_.column < row_change_position)
			? (this->fetch_pointer_.row + this->mode_timing_.total_lines - 1) % this->mode_timing_.total_lines
			: this->fetch_pointer_.row;
}

template <Personality personality>
bool Base<personality>::is_vertical_blank() const {
	return fetch_pointer_.row >= mode_timing_.pixel_lines && fetch_pointer_.row != mode_timing_.total_lines - 1;
}

template <Personality personality>
bool Base<personality>::is_horizontal_blank() const {
	return fetch_pointer_.column < StandardTiming<personality>::FirstPixelCycle;
}

template <Personality personality>
uint8_t TMS9918<personality>::get_current_line() const {
	int source_row = this->fetch_line();

	if(this->tv_standard_ == TVStandard::NTSC) {
		if(this->mode_timing_.pixel_lines == 240) {
			// NTSC 256x240:	00-FF, 00-06
		} else if(this->mode_timing_.pixel_lines == 224) {
			// NTSC 256x224:	00-EA, E5-FF
			if(source_row >= 0xeb) source_row -= 6;
		} else {
			// NTSC 256x192:	00-DA, D5-FF
			if(source_row >= 0xdb) source_row -= 6;
		}
	} else {
		if(this->mode_timing_.pixel_lines == 240) {
			// PAL 256x240:		00-FF, 00-0A, D2-FF
			if(source_row >= 267) source_row -= 0x39;
		} else if(this->mode_timing_.pixel_lines == 224) {
			// PAL 256x224:		00-FF, 00-02, CA-FF
			if(source_row >= 259) source_row -= 0x39;
		} else {
			// PAL 256x192:		00-F2, BA-FF
			if(source_row >= 0xf3) source_row -= 0x39;
		}
	}

	return uint8_t(source_row);
}
template <Personality personality>
HalfCycles TMS9918<personality>::get_next_sequence_point() const {
	if(!this->generate_interrupts_ && !this->enable_line_interrupts_) return HalfCycles::max();
	if(get_interrupt_line()) return HalfCycles::max();

	// Calculate the amount of time until the next end-of-frame interrupt.
	const int frame_length = Timing<personality>::CyclesPerLine * this->mode_timing_.total_lines;
	int time_until_frame_interrupt =
		(
			((this->mode_timing_.end_of_frame_interrupt_position.row * Timing<personality>::CyclesPerLine) + this->mode_timing_.end_of_frame_interrupt_position.column + frame_length) -
			((this->fetch_pointer_.row * Timing<personality>::CyclesPerLine) + this->fetch_pointer_.column)
		) % frame_length;
	if(!time_until_frame_interrupt) time_until_frame_interrupt = frame_length;

	if(!this->enable_line_interrupts_) {
		return this->clock_converter_.half_cycles_before_internal_cycles(time_until_frame_interrupt);
	}

	// Calculate when the next line interrupt will occur.
	int next_line_interrupt_row = -1;

	int cycles_to_next_interrupt_threshold = this->mode_timing_.line_interrupt_position - this->fetch_pointer_.column;
	int line_of_next_interrupt_threshold = this->fetch_pointer_.row;
	if(cycles_to_next_interrupt_threshold <= 0) {
		cycles_to_next_interrupt_threshold += Timing<personality>::CyclesPerLine;
		++line_of_next_interrupt_threshold;
	}

	if constexpr (is_sega_vdp(personality)) {
		// If there is still time for a line interrupt this frame, that'll be it;
		// otherwise it'll be on the next frame, supposing there's ever time for
		// it at all.
		if(line_of_next_interrupt_threshold + this->line_interrupt_counter_ <= this->mode_timing_.pixel_lines) {
			next_line_interrupt_row = line_of_next_interrupt_threshold + this->line_interrupt_counter_;
		} else {
			if(this->line_interrupt_target_ <= this->mode_timing_.pixel_lines)
				next_line_interrupt_row = this->mode_timing_.total_lines + this->line_interrupt_target_;
		}
	}

	// If there's actually no interrupt upcoming, despite being enabled, either return
	// the frame end interrupt or no interrupt pending as appropriate.
	if(next_line_interrupt_row == -1) {
		return this->generate_interrupts_ ?
			this->clock_converter_.half_cycles_before_internal_cycles(time_until_frame_interrupt) :
			HalfCycles::max();
	}

	// Figure out the number of internal cycles until the next line interrupt, which is the amount
	// of time to the next tick over and then next_line_interrupt_row - row_ lines further.
	const int local_cycles_until_line_interrupt = cycles_to_next_interrupt_threshold + (next_line_interrupt_row - line_of_next_interrupt_threshold) * Timing<personality>::CyclesPerLine;
	if(!this->generate_interrupts_) return this->clock_converter_.half_cycles_before_internal_cycles(local_cycles_until_line_interrupt);

	// Return whichever interrupt is closer.
	return this->clock_converter_.half_cycles_before_internal_cycles(std::min(local_cycles_until_line_interrupt, time_until_frame_interrupt));
}

template <Personality personality>
HalfCycles TMS9918<personality>::get_time_until_line(int line) {
	if(line < 0) line += this->mode_timing_.total_lines;

	int cycles_to_next_interrupt_threshold = this->mode_timing_.line_interrupt_position - this->fetch_pointer_.column;
	int line_of_next_interrupt_threshold = this->fetch_pointer_.row;
	if(cycles_to_next_interrupt_threshold <= 0) {
		cycles_to_next_interrupt_threshold += Timing<personality>::CyclesPerLine;
		++line_of_next_interrupt_threshold;
	}

	if(line_of_next_interrupt_threshold > line) {
		line += this->mode_timing_.total_lines;
	}

	return this->clock_converter_.half_cycles_before_internal_cycles(cycles_to_next_interrupt_threshold + (line - line_of_next_interrupt_threshold)*Timing<personality>::CyclesPerLine);
}

template <Personality personality>
bool TMS9918<personality>::get_interrupt_line() const {
	return
		((this->status_ & StatusInterrupt) && this->generate_interrupts_) ||
		(this->enable_line_interrupts_ && this->line_interrupt_pending_);
}

// TODO: [potentially] remove Master System timing assumptions in latch and get_latched below.
template <Personality personality>uint8_t TMS9918<personality>::get_latched_horizontal_counter() const {
	// Translate from internal numbering, which puts pixel output
	// in the final 256 pixels of 342, to the public numbering,
	// which counts the 256 pixels as items 0–255, starts
	// counting at -48, and returns only the top 8 bits of the number.
	int public_counter = this->latched_column_ - (342 - 256);
	if(public_counter < -46) public_counter += 342;
	return uint8_t(public_counter >> 1);
}

template <Personality personality>
void TMS9918<personality>::latch_horizontal_counter() {
	this->latched_column_ = this->fetch_pointer_.column;
}

template class TI::TMS::TMS9918<Personality::TMS9918A>;
template class TI::TMS::TMS9918<Personality::V9938>;
//template class TI::TMS::TMS9918<Personality::V9958>;
template class TI::TMS::TMS9918<Personality::SMSVDP>;
template class TI::TMS::TMS9918<Personality::SMS2VDP>;
//template class TI::TMS::TMS9918<Personality::GGVDP>;
//template class TI::TMS::TMS9918<Personality::MDVDP>;
