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
	read_pointer_.row = rand() % 262;
	read_pointer_.column = rand() % (Timing<personality>::CyclesPerLine - output_lag);
	write_pointer_.row = read_pointer_.row;
	write_pointer_.column = read_pointer_.column + output_lag;
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
		LineBufferPointer backup = this->read_pointer_;
#endif

		if(write_cycles_pool) {
			// Determine how much writing to do.
			const int write_cycles = std::min(
				Timing<personality>::CyclesPerLine - this->write_pointer_.column,
				write_cycles_pool
			);
			const int end_column = this->write_pointer_.column + write_cycles;
			LineBuffer &line_buffer = this->line_buffers_[this->write_pointer_.row];

			// Determine what this does to any enqueued VRAM access.
			this->minimum_access_column_ = this->write_pointer_.column + this->cycles_until_access_;
			this->cycles_until_access_ -= write_cycles;


			// ---------------------------------------
			// Latch scrolling position, if necessary.
			// ---------------------------------------
			if constexpr (is_sega_vdp(personality)) {
				if(this->write_pointer_.column < 61 && end_column >= 61) {
					if(!this->write_pointer_.row) {
						this->master_system_.latched_vertical_scroll = this->master_system_.vertical_scroll;

						if(this->master_system_.mode4_enable) {
							this->mode_timing_.pixel_lines = 192;
							if(this->mode2_enable_ && this->mode1_enable_) this->mode_timing_.pixel_lines = 224;
							if(this->mode2_enable_ && this->mode3_enable_) this->mode_timing_.pixel_lines = 240;

							this->mode_timing_.allow_sprite_terminator = this->mode_timing_.pixel_lines == 192;
							this->mode_timing_.first_vsync_line = (this->mode_timing_.total_lines + this->mode_timing_.pixel_lines) >> 1;

							this->mode_timing_.end_of_frame_interrupt_position.row = this->mode_timing_.pixel_lines + 1;
						}
					}
					line_buffer.latched_horizontal_scroll = this->master_system_.horizontal_scroll;
				}
			}



			// ------------------------
			// Perform memory accesses.
			// ------------------------
#define fetch(function, converter)															\
	const int first_window = this->clock_converter_.converter(this->write_pointer_.column);	\
	const int final_window = this->clock_converter_.converter(end_column);					\
	if(first_window == final_window) break;													\
	if(final_window != TMSAccessWindowsPerLine) {											\
		function<true>(first_window, final_window);											\
	} else {																				\
		function<false>(first_window, final_window);										\
	}

			switch(line_buffer.line_mode) {
				case LineMode::Text:		{	fetch(this->template fetch_tms_text, to_tms_access_clock);		}	break;
				case LineMode::Character:	{	fetch(this->template fetch_tms_character, to_tms_access_clock);	}	break;
				case LineMode::SMS:			{	fetch(this->template fetch_sms, to_tms_access_clock);			}	break;
				case LineMode::Refresh:		{	fetch(this->template fetch_tms_refresh, to_tms_access_clock);	}	break;
			}

#undef fetch



			// -------------------------------
			// Check for interrupt conditions.
			// -------------------------------
			if(this->write_pointer_.column < this->mode_timing_.line_interrupt_position && end_column >= this->mode_timing_.line_interrupt_position) {
				// The Sega VDP offers a decrementing counter for triggering line interrupts;
				// it is reloaded either when it overflows or upon every non-pixel line after the first.
				// It is otherwise decremented.
				if constexpr (is_sega_vdp(personality)) {
					if(this->write_pointer_.row >= 0 && this->write_pointer_.row <= this->mode_timing_.pixel_lines) {
						--this->line_interrupt_counter;
						if(this->line_interrupt_counter == 0xff) {
							this->line_interrupt_pending_ = true;
							this->line_interrupt_counter = this->line_interrupt_target;
						}
					} else {
						this->line_interrupt_counter = this->line_interrupt_target;
					}
				}

				// TODO: the V9938 provides line interrupts from direct specification of the target line.
				// So life is easy.
			}

			if(
				this->write_pointer_.row == this->mode_timing_.end_of_frame_interrupt_position.row &&
				this->write_pointer_.column < this->mode_timing_.end_of_frame_interrupt_position.column &&
				end_column >= this->mode_timing_.end_of_frame_interrupt_position.column
			) {
				this->status_ |= StatusInterrupt;
			}



			// -------------
			// Advance time.
			// -------------
			this->write_pointer_.column = end_column;
			write_cycles_pool -= write_cycles;

			if(this->write_pointer_.column == Timing<personality>::CyclesPerLine) {
				this->write_pointer_.column = 0;
				this->write_pointer_.row = (this->write_pointer_.row + 1) % this->mode_timing_.total_lines;
				LineBuffer &next_line_buffer = this->line_buffers_[this->write_pointer_.row];

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
					(this->write_pointer_.row >= this->mode_timing_.pixel_lines && this->write_pointer_.row != this->mode_timing_.total_lines-1))
						next_line_buffer.line_mode = LineMode::Refresh;
			}
		}


#ifndef NDEBUG
		assert(backup.row == this->read_pointer_.row && backup.column == this->read_pointer_.column);
		backup = this->write_pointer_;
#endif


		if(read_cycles_pool) {
			// Determine how much time has passed in the remainder of this line, and proceed.
			const int target_read_cycles = std::min(
				Timing<personality>::CyclesPerLine - this->read_pointer_.column,
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

					if(!this->upcoming_cram_dots_.empty() && this->upcoming_cram_dots_.front().location.row == this->read_pointer_.row) {
						int time_until_dot = this->upcoming_cram_dots_.front().location.column - this->read_pointer_.column;

						if(time_until_dot < read_cycles) {
							read_cycles = time_until_dot;
							next_cram_value = this->upcoming_cram_dots_.front().value;
							this->upcoming_cram_dots_.erase(this->upcoming_cram_dots_.begin());
						}
					}
				}

				read_cycles_performed += read_cycles;

				const int end_column = this->read_pointer_.column + read_cycles;
				LineBuffer &line_buffer = this->line_buffers_[this->read_pointer_.row];


				// --------------------
				// Output video stream.
				// --------------------

#define crt_convert(action, time)		this->crt_.action(this->clock_converter_.to_crt_clock(time))
#define output_sync(x)					crt_convert(output_sync, x)
#define output_blank(x)					crt_convert(output_blank, x)
#define output_default_colour_burst(x)	crt_convert(output_default_colour_burst, x)

#define intersect(left, right, code)	{	\
		const int start = std::max(this->read_pointer_.column, left);	\
		const int end = std::min(end_column, right);	\
		if(end > start) {\
			code;\
		}\
	}

#define border(left, right)	intersect(left, right, this->output_border(end - start, cram_value))

				if(line_buffer.line_mode == LineMode::Refresh || this->read_pointer_.row > this->mode_timing_.pixel_lines) {
					if(
						this->read_pointer_.row >= this->mode_timing_.first_vsync_line &&
						this->read_pointer_.row < this->mode_timing_.first_vsync_line + 4
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
							this->read_pointer_.column < Timing<personality>::StartOfLeftBorder &&
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
						this->read_pointer_.column < Timing<personality>::StartOfLeftBorder &&
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

#define draw(function, converter) {																				\
	const int relative_start = this->clock_converter_.converter(start - line_buffer.first_pixel_output_column);	\
	const int relative_end = this->clock_converter_.converter(end - line_buffer.first_pixel_output_column);		\
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
								case LineMode::SMS:			draw(draw_sms(relative_start, relative_end, cram_value), to_tms_pixel_clock);	break;
								case LineMode::Character:	draw(draw_tms_character(relative_start, relative_end), to_tms_pixel_clock);		break;
								case LineMode::Text:		draw(draw_tms_text(relative_start, relative_end), to_tms_pixel_clock);			break;

								case LineMode::Refresh:		break;	/* Dealt with elsewhere. */
							}
						}

						if(end == line_buffer.next_border_column) {
							const int length = line_buffer.next_border_column - line_buffer.first_pixel_output_column;
							this->crt_.output_data(this->clock_converter_.to_crt_clock(length), line_buffer.pixel_count);
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
				this->read_pointer_.column = end_column;
			}

			read_cycles_pool -= target_read_cycles;
			if(this->read_pointer_.column == Timing<personality>::CyclesPerLine) {
				this->read_pointer_.column = 0;
				this->read_pointer_.row = (this->read_pointer_.row + 1) % this->mode_timing_.total_lines;
			}
		}

		assert(backup.row == this->write_pointer_.row && backup.column == this->write_pointer_.column);
	}
}

template <Personality personality>
void Base<personality>::output_border(int cycles, [[maybe_unused]] uint32_t cram_dot) {
	cycles = this->clock_converter_.to_crt_clock(cycles);
	const uint32_t border_colour =
		is_sega_vdp(personality) ?
			master_system_.colour_ram[16 + background_colour_] :
			palette[background_colour_];

	if constexpr (is_sega_vdp(personality)) {
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

template <Personality personality>
void TMS9918<personality>::write(int address, uint8_t value) {
	// Writes to address 0 are writes to the video RAM. Store
	// the value and return.
	if(!(address & 1)) {
		this->write_phase_ = false;

		// Enqueue the write to occur at the next available slot.
		this->read_ahead_buffer_ = value;
		this->queued_access_ = MemoryAccess::Write;
		this->cycles_until_access_ = Timing<personality>::VRAMAccessDelay;

		return;
	}

	// Writes to address 1 are performed in pairs; if this is the
	// low byte of a value, store it and wait for the high byte.
	if(!this->write_phase_) {
		this->low_write_ = value;
		this->write_phase_ = true;

		// The initial write should half update the access pointer.
		this->ram_pointer_ = (this->ram_pointer_ & 0xff00) | this->low_write_;
		return;
	}

	// The RAM pointer is always set on a second write, regardless of
	// whether the caller is intending to enqueue a VDP operation.
	this->ram_pointer_ = (this->ram_pointer_ & 0x00ff) | uint16_t(value << 8);

	this->write_phase_ = false;
	if(value & 0x80) {
		if constexpr (is_sega_vdp(personality)) {
			if(value & 0x40) {
				this->master_system_.cram_is_selected = true;
				return;
			}
			value &= 0xf;
		} else {
			value &= 0x7;
		}

		// This is a write to a register.
		switch(value) {
			case 0:
				if constexpr (is_sega_vdp(personality)) {
					this->master_system_.vertical_scroll_lock = this->low_write_ & 0x80;
					this->master_system_.horizontal_scroll_lock = this->low_write_ & 0x40;
					this->master_system_.hide_left_column = this->low_write_ & 0x20;
					this->enable_line_interrupts_ = this->low_write_ & 0x10;
					this->master_system_.shift_sprites_8px_left = this->low_write_ & 0x08;
					this->master_system_.mode4_enable = this->low_write_ & 0x04;
				}
				this->mode2_enable_ = this->low_write_ & 0x02;
			break;

			case 1:
				this->blank_display_ = !(this->low_write_ & 0x40);
				this->generate_interrupts_ = this->low_write_ & 0x20;
				this->mode1_enable_ = this->low_write_ & 0x10;
				this->mode3_enable_ = this->low_write_ & 0x08;
				this->sprites_16x16_ = this->low_write_ & 0x02;
				this->sprites_magnified_ = this->low_write_ & 0x01;

				this->sprite_height_ = 8;
				if(this->sprites_16x16_) this->sprite_height_ <<= 1;
				if(this->sprites_magnified_) this->sprite_height_ <<= 1;
			break;

			case 2:
				this->pattern_name_address_ = size_t((this->low_write_ & 0xf) << 10) | 0x3ff;
				this->master_system_.pattern_name_address = this->pattern_name_address_ | ((personality == TMS::SMSVDP) ? 0x000 : 0x400);
			break;

			case 3:
				this->colour_table_address_ = size_t(this->low_write_ << 6) | 0x3f;
			break;

			case 4:
				this->pattern_generator_table_address_ = size_t((this->low_write_ & 0x07) << 11) | 0x7ff;
			break;

			case 5:
				this->sprite_attribute_table_address_ = size_t((this->low_write_ & 0x7f) << 7) | 0x7f;
				this->master_system_.sprite_attribute_table_address = this->sprite_attribute_table_address_ | ((personality == TMS::SMSVDP) ? 0x00 : 0x80);
			break;

			case 6:
				this->sprite_generator_table_address_ = size_t((this->low_write_ & 0x07) << 11) | 0x7ff;
				this->master_system_.sprite_generator_table_address = this->sprite_generator_table_address_ | ((personality == TMS::SMSVDP) ? 0x0000 : 0x1800);
			break;

			case 7:
				this->text_colour_ = this->low_write_ >> 4;
				this->background_colour_ = this->low_write_ & 0xf;
			break;

			case 8:
				if constexpr (is_sega_vdp(personality)) {
					this->master_system_.horizontal_scroll = this->low_write_;
				}
			break;

			case 9:
				if constexpr (is_sega_vdp(personality)) {
					this->master_system_.vertical_scroll = this->low_write_;
				}
			break;

			case 10:
				if constexpr (is_sega_vdp(personality)) {
					this->line_interrupt_target = this->low_write_;
				}
			break;

			default:
				LOG("Unknown TMS write: " << int(this->low_write_) << " to " << int(value));
			break;
		}
	} else {
		// This is an access via the RAM pointer.
		if(!(value & 0x40)) {
			// A read request is enqueued upon setting the address; conversely a write
			// won't be enqueued unless and until some actual data is supplied.
			this->queued_access_ = MemoryAccess::Read;
			this->cycles_until_access_ = Timing<personality>::VRAMAccessDelay;
		}
		this->master_system_.cram_is_selected = false;
	}
}

template <Personality personality>
uint8_t TMS9918<personality>::get_current_line() const {
	// Determine the row to return.
	constexpr int row_change_position = 63;	// This is the proper Master System value; substitute if any other VDPs turn out to have this functionality.
	int source_row =
		(this->write_pointer_.column < row_change_position)
			? (this->write_pointer_.row + this->mode_timing_.total_lines - 1) % this->mode_timing_.total_lines
			: this->write_pointer_.row;

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
uint8_t TMS9918<personality>::read(int address) {
	this->write_phase_ = false;

	// Reads from address 0 read video RAM, via the read-ahead buffer.
	if(!(address & 1)) {
		// Enqueue the write to occur at the next available slot.
		const uint8_t result = this->read_ahead_buffer_;
		this->queued_access_ = MemoryAccess::Read;
		return result;
	}

	// Reads from address 1 get the status register.
	const uint8_t result = this->status_;
	this->status_ &= ~(StatusInterrupt | StatusSpriteOverflow | StatusSpriteCollision);
	this->line_interrupt_pending_ = false;
	return result;
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
			((this->write_pointer_.row * Timing<personality>::CyclesPerLine) + this->write_pointer_.column)
		) % frame_length;
	if(!time_until_frame_interrupt) time_until_frame_interrupt = frame_length;

	if(!this->enable_line_interrupts_) {
		return this->clock_converter_.half_cycles_before_internal_cycles(time_until_frame_interrupt);
	}

	// Calculate when the next line interrupt will occur.
	int next_line_interrupt_row = -1;

	int cycles_to_next_interrupt_threshold = this->mode_timing_.line_interrupt_position - this->write_pointer_.column;
	int line_of_next_interrupt_threshold = this->write_pointer_.row;
	if(cycles_to_next_interrupt_threshold <= 0) {
		cycles_to_next_interrupt_threshold += Timing<personality>::CyclesPerLine;
		++line_of_next_interrupt_threshold;
	}

	if constexpr (is_sega_vdp(personality)) {
		// If there is still time for a line interrupt this frame, that'll be it;
		// otherwise it'll be on the next frame, supposing there's ever time for
		// it at all.
		if(line_of_next_interrupt_threshold + this->line_interrupt_counter <= this->mode_timing_.pixel_lines) {
			next_line_interrupt_row = line_of_next_interrupt_threshold + this->line_interrupt_counter;
		} else {
			if(this->line_interrupt_target <= this->mode_timing_.pixel_lines)
				next_line_interrupt_row = this->mode_timing_.total_lines + this->line_interrupt_target;
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

	int cycles_to_next_interrupt_threshold = this->mode_timing_.line_interrupt_position - this->write_pointer_.column;
	int line_of_next_interrupt_threshold = this->write_pointer_.row;
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
	this->latched_column_ = this->write_pointer_.column;
}

template class TI::TMS::TMS9918<Personality::TMS9918A>;
template class TI::TMS::TMS9918<Personality::V9938>;
template class TI::TMS::TMS9918<Personality::V9958>;
template class TI::TMS::TMS9918<Personality::SMSVDP>;
template class TI::TMS::TMS9918<Personality::SMS2VDP>;
template class TI::TMS::TMS9918<Personality::GGVDP>;
template class TI::TMS::TMS9918<Personality::MDVDP>;
