//
//  9918.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/11/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "9918.hpp"

#include <cassert>
#include <cstring>
#include <cstdlib>
#include "../../Outputs/Log.hpp"

using namespace TI::TMS;

//
// TODO notes.
//
//	The TMS and Master System VDP run at 342 cycles/line.
//
//	The Mega Drive VDP has 3420 master clocks per line, which it divides
//	by 4 or 5 depending on pixel rate and which part of a line is active.
//	(And, presumably, by 10 if operating in Master System mode?)
//	Cf. https://gendev.spritesmind.net/forum/viewtopic.php?t=851 etc.
//
//	The MSX2+ Yamaha VDPs have 1368 cycles/line.
//
//	So if clock scaling were common, it would need to be to:
//
//		3420 = 2 * 2		* 	3 * 3	*	5	*	19
//		1368 = 2 * 2 * 2	*	3 * 3	* 			19
//
//	=> 2^3 * 3^2 * 5 * 19 = 6840
//		... which would imply a multiply by 30 on the input clock if
//		it were to remain 3.58Mhz.

// Mega Drive notes, assorted:
//
//
//
// there are 60 EDCLK at MCLK/5 during a line ... like this:
//
//	15 @ /5; 2 @ /4; 15 @ /5; 2 @ /4; 15 @ /5; 2 @ /4; 15 @ /5
//
// ... HSYNC signal is what triggers the changes in EDCLK frequency, with some latency though.
//
// The total is still 840 EDCLKS (420 pixels) with 780 @MCLK/4 and 60@MCLK/5 (total is 3420 MCLCKS).

namespace {

constexpr uint8_t StatusInterrupt = 0x80;
constexpr uint8_t StatusSpriteOverflow = 0x40;

constexpr int StatusSpriteCollisionShift = 5;
constexpr uint8_t StatusSpriteCollision = 0x20;

// 342 internal cycles are 228/227.5ths of a line, so 341.25 cycles should be a whole
// line. Therefore multiply everything by four, but set line length to 1365 rather than 342*4 = 1368.
constexpr unsigned int CRTCyclesPerLine = 1365;
constexpr unsigned int CRTCyclesDivider = 4;

struct ReverseTable {
	const std::array<uint8_t, 256> map;
	constexpr ReverseTable() : map(reverse_table()) {}

	private:
		static constexpr std::array<uint8_t, 256> reverse_table() {
			std::array<uint8_t, 256> map{};
			for(size_t c = 0; c < 256; ++c) {
				map[c] = uint8_t(
					((c & 0x80) >> 7) |
					((c & 0x40) >> 5) |
					((c & 0x20) >> 3) |
					((c & 0x10) >> 1) |
					((c & 0x08) << 1) |
					((c & 0x04) << 3) |
					((c & 0x02) << 5) |
					((c & 0x01) << 7)
				);
			}
			return map;
		}
};

constexpr ReverseTable reverse_table;

constexpr size_t memory_size(Personality p) {
	switch(p) {
		case TI::TMS::TMS9918A:
		case TI::TMS::SMSVDP:
		case TI::TMS::SMS2VDP:
		case TI::TMS::GGVDP:	return 16 * 1024;
		case TI::TMS::MDVDP:	return 64 * 1024;
		case TI::TMS::V9938:	return 128 * 1024;
		case TI::TMS::V9958:	return 192 * 1024;
	}
}

}

Base::Base(Personality p) :
	personality_(p),
	crt_(CRTCyclesPerLine, CRTCyclesDivider, Outputs::Display::Type::NTSC60, Outputs::Display::InputDataType::Red8Green8Blue8) {
	// Unimaginatively, this class just passes RGB through to the shader. Investigation is needed
	// into whether there's a more natural form. It feels unlikely given the diversity of chips modelled.

	ram_.resize(memory_size(p));
	if(is_sega_vdp(personality_)) {
		mode_timing_.line_interrupt_position = 64;

		mode_timing_.end_of_frame_interrupt_position.column = 63;
		mode_timing_.end_of_frame_interrupt_position.row = 193;
	}

	// Establish that output is delayed after reading by `output_lag` cycles; start
	// at a random position.
	read_pointer_.row = rand() % 262;
	read_pointer_.column = rand() % (342 - output_lag);
	write_pointer_.row = read_pointer_.row;
	write_pointer_.column = read_pointer_.column + output_lag;
}

TMS9918::TMS9918(Personality p):
	Base(p) {
	crt_.set_display_type(Outputs::Display::DisplayType::RGB);
	crt_.set_visible_area(Outputs::Display::Rect(0.07f, 0.0375f, 0.875f, 0.875f));

	// The TMS remains in-phase with the NTSC colour clock; this is an empirical measurement
	// intended to produce the correct relationship between the hard edges between pixels and
	// the colour clock. It was eyeballed rather than derived from any knowledge of the TMS
	// colour burst generator because I've yet to find any.
	crt_.set_immediate_default_phase(0.85f);
}

void TMS9918::set_tv_standard(TVStandard standard) {
	tv_standard_ = standard;
	switch(standard) {
		case TVStandard::PAL:
			mode_timing_.total_lines = 313;
			mode_timing_.first_vsync_line = 253;
			crt_.set_new_display_type(CRTCyclesPerLine, Outputs::Display::Type::PAL50);
		break;
		default:
			mode_timing_.total_lines = 262;
			mode_timing_.first_vsync_line = 227;
			crt_.set_new_display_type(CRTCyclesPerLine, Outputs::Display::Type::NTSC60);
		break;
	}
}

void TMS9918::set_scan_target(Outputs::Display::ScanTarget *scan_target) {
	crt_.set_scan_target(scan_target);
}

Outputs::Display::ScanStatus TMS9918::get_scaled_scan_status() const {
	// The input was scaled by 3/4 to convert half cycles to internal ticks,
	// so undo that and also allow for: (i) the multiply by 4 that it takes
	// to reach the CRT; and (ii) the fact that the half-cycles value was scaled,
	// and this should really reply in whole cycles.
	return crt_.get_scaled_scan_status() * (4.0f / (3.0f * 8.0f));
}

void TMS9918::set_display_type(Outputs::Display::DisplayType display_type) {
	crt_.set_display_type(display_type);
}

Outputs::Display::DisplayType TMS9918::get_display_type() const {
	return crt_.get_display_type();
}

void Base::LineBuffer::reset_sprite_collection() {
	sprites_stopped = false;
	active_sprite_slot = 0;

	for(int c = 0; c < 8; ++c) {
		active_sprites[c].shift_position = 0;
	}
}

void Base::posit_sprite(LineBuffer &buffer, int sprite_number, int sprite_position, int screen_row) {
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

void TMS9918::run_for(const HalfCycles cycles) {
	// As specific as I've been able to get:
	// Scanline time is always 228 cycles.
	// PAL output is 313 lines total. NTSC output is 262 lines total.
	// Interrupt is signalled upon entering the lower border.

	// Convert 456 clocked half cycles per line to 342 internal cycles per line;
	// the internal clock is 1.5 times the nominal 3.579545 Mhz that I've advertised
	// for this part. So multiply by three quarters.
	int int_cycles = int(cycles.as_integral() * 3) + cycles_error_;
	cycles_error_ = int_cycles & 3;
	int_cycles >>= 2;
	if(!int_cycles) return;

	// There are two intertwined processes here, 'writing' (which means writing to the
	// line buffers, i.e. it's everything to do with collecting a line) and 'reading'
	// (which means reading from the line buffers and generating video).
	int write_cycles_pool = int_cycles;
	int read_cycles_pool = int_cycles;

	while(write_cycles_pool || read_cycles_pool) {
#ifndef NDEBUG
		LineBufferPointer backup = read_pointer_;
#endif

		if(write_cycles_pool) {
			// Determine how much writing to do.
			const int write_cycles = std::min(342 - write_pointer_.column, write_cycles_pool);
			const int end_column = write_pointer_.column + write_cycles;
			LineBuffer &line_buffer = line_buffers_[write_pointer_.row];

			// Determine what this does to any enqueued VRAM access.
			minimum_access_column_ = write_pointer_.column + cycles_until_access_;
			cycles_until_access_ -= write_cycles;


			// ---------------------------------------
			// Latch scrolling position, if necessary.
			// ---------------------------------------
			if(is_sega_vdp(personality_)) {
				if(write_pointer_.column < 61 && end_column >= 61) {
					if(!write_pointer_.row) {
						master_system_.latched_vertical_scroll = master_system_.vertical_scroll;

						if(master_system_.mode4_enable) {
							mode_timing_.pixel_lines = 192;
							if(mode2_enable_ && mode1_enable_) mode_timing_.pixel_lines = 224;
							if(mode2_enable_ && mode3_enable_) mode_timing_.pixel_lines = 240;

							mode_timing_.allow_sprite_terminator = mode_timing_.pixel_lines == 192;
							mode_timing_.first_vsync_line = (mode_timing_.total_lines + mode_timing_.pixel_lines) >> 1;

							mode_timing_.end_of_frame_interrupt_position.row = mode_timing_.pixel_lines + 1;
						}
					}
					line_buffer.latched_horizontal_scroll = master_system_.horizontal_scroll;
				}
			}



			// ------------------------
			// Perform memory accesses.
			// ------------------------
#define fetch(function)	\
	if(final_window != 171) {	\
		function<true>(first_window, final_window);\
	} else {\
		function<false>(first_window, final_window);\
	}

			// column_ and end_column are in 342-per-line cycles;
			// adjust them to a count of windows.
			const int first_window = write_pointer_.column >> 1;
			const int final_window = end_column >> 1;
			if(first_window != final_window) {
				switch(line_buffer.line_mode) {
					case LineMode::Text:		fetch(fetch_tms_text);		break;
					case LineMode::Character:	fetch(fetch_tms_character);	break;
					case LineMode::SMS:			fetch(fetch_sms);			break;
					case LineMode::Refresh:		fetch(fetch_tms_refresh);	break;
				}
			}

#undef fetch



			// -------------------------------
			// Check for interrupt conditions.
			// -------------------------------
			if(write_pointer_.column < mode_timing_.line_interrupt_position && end_column >= mode_timing_.line_interrupt_position) {
				// The Sega VDP offers a decrementing counter for triggering line interrupts;
				// it is reloaded either when it overflows or upon every non-pixel line after the first.
				// It is otherwise decremented.
				if(is_sega_vdp(personality_)) {
					if(write_pointer_.row >= 0 && write_pointer_.row <= mode_timing_.pixel_lines) {
						--line_interrupt_counter;
						if(line_interrupt_counter == 0xff) {
							line_interrupt_pending_ = true;
							line_interrupt_counter = line_interrupt_target;
						}
					} else {
						line_interrupt_counter = line_interrupt_target;
					}
				}

				// TODO: the V9938 provides line interrupts from direct specification of the target line.
				// So life is easy.
			}

			if(
				write_pointer_.row == mode_timing_.end_of_frame_interrupt_position.row &&
				write_pointer_.column < mode_timing_.end_of_frame_interrupt_position.column &&
				end_column >= mode_timing_.end_of_frame_interrupt_position.column
			) {
				status_ |= StatusInterrupt;
			}



			// -------------
			// Advance time.
			// -------------
			write_pointer_.column = end_column;
			write_cycles_pool -= write_cycles;

			if(write_pointer_.column == 342) {
				write_pointer_.column = 0;
				write_pointer_.row = (write_pointer_.row + 1) % mode_timing_.total_lines;
				LineBuffer &next_line_buffer = line_buffers_[write_pointer_.row];

				// Establish the output mode for the next line.
				set_current_screen_mode();

				// Based on the output mode, pick a line mode.
				next_line_buffer.first_pixel_output_column = 86;
				next_line_buffer.next_border_column = 342;
				mode_timing_.maximum_visible_sprites = 4;
				switch(screen_mode_) {
					case ScreenMode::Text:
						next_line_buffer.line_mode = LineMode::Text;
						next_line_buffer.first_pixel_output_column = 94;
						next_line_buffer.next_border_column = 334;
					break;
					case ScreenMode::SMSMode4:
						next_line_buffer.line_mode = LineMode::SMS;
						mode_timing_.maximum_visible_sprites = 8;
					break;
					default:
						next_line_buffer.line_mode = LineMode::Character;
					break;
				}

				if(
					(screen_mode_ == ScreenMode::Blank) ||
					(write_pointer_.row >= mode_timing_.pixel_lines && write_pointer_.row != mode_timing_.total_lines-1))
						next_line_buffer.line_mode = LineMode::Refresh;
			}
		}


#ifndef NDEBUG
		assert(backup.row == read_pointer_.row && backup.column == read_pointer_.column);
		backup = write_pointer_;
#endif


		if(read_cycles_pool) {
			// Determine how much time has passed in the remainder of this line, and proceed.
			const int target_read_cycles = std::min(342 - read_pointer_.column, read_cycles_pool);
			int read_cycles_performed = 0;
			uint32_t next_cram_value = 0;

			while(read_cycles_performed < target_read_cycles) {
				const uint32_t cram_value = next_cram_value;
				next_cram_value = 0;
				int read_cycles = target_read_cycles - read_cycles_performed;
				if(!upcoming_cram_dots_.empty() && upcoming_cram_dots_.front().location.row == read_pointer_.row) {
					int time_until_dot = upcoming_cram_dots_.front().location.column - read_pointer_.column;

					if(time_until_dot < read_cycles) {
						read_cycles = time_until_dot;
						next_cram_value = upcoming_cram_dots_.front().value;
						upcoming_cram_dots_.erase(upcoming_cram_dots_.begin());
					}
				}

				if(!read_cycles) continue;
				read_cycles_performed += read_cycles;

				const int end_column = read_pointer_.column + read_cycles;
				LineBuffer &line_buffer = line_buffers_[read_pointer_.row];


				// --------------------
				// Output video stream.
				// --------------------

#define intersect(left, right, code)	{	\
		const int start = std::max(read_pointer_.column, left);	\
		const int end = std::min(end_column, right);	\
		if(end > start) {\
			code;\
		}\
	}

#define border(left, right)	intersect(left, right, output_border(end - start, cram_value))

				if(line_buffer.line_mode == LineMode::Refresh || read_pointer_.row > mode_timing_.pixel_lines) {
					if(read_pointer_.row >= mode_timing_.first_vsync_line && read_pointer_.row < mode_timing_.first_vsync_line+4) {
						// Vertical sync.
						if(end_column == 342) {
							crt_.output_sync(342 * 4);
						}
					} else {
						// Right border.
						border(0, 15);

						// Blanking region; total length is 58 cycles,
						// and 58+15 = 73. So output the lot when the
						// cursor passes 73.
						if(read_pointer_.column < 73 && end_column >= 73) {
							crt_.output_blank(8*4);
							crt_.output_sync(26*4);
							crt_.output_blank(2*4);
							crt_.output_default_colour_burst(14*4);
							crt_.output_blank(8*4);
						}

						// Border colour for the rest of the line.
						border(73, 342);
					}
				} else {
					// Right border.
					border(0, 15);

					// Blanking region.
					if(read_pointer_.column < 73 && end_column >= 73) {
						crt_.output_blank(8*4);
						crt_.output_sync(26*4);
						crt_.output_blank(2*4);
						crt_.output_default_colour_burst(14*4);
						crt_.output_blank(8*4);
					}

					// Left border.
					border(73, line_buffer.first_pixel_output_column);

					// Pixel region.
					intersect(
						line_buffer.first_pixel_output_column,
						line_buffer.next_border_column,
						if(!asked_for_write_area_) {
							asked_for_write_area_ = true;
							pixel_origin_ = pixel_target_ = reinterpret_cast<uint32_t *>(
								crt_.begin_data(size_t(line_buffer.next_border_column - line_buffer.first_pixel_output_column))
							);
						}

						if(pixel_target_) {
							const int relative_start = start - line_buffer.first_pixel_output_column;
							const int relative_end = end - line_buffer.first_pixel_output_column;
							switch(line_buffer.line_mode) {
								case LineMode::SMS:			draw_sms(relative_start, relative_end, cram_value);		break;
								case LineMode::Character:	draw_tms_character(relative_start, relative_end);		break;
								case LineMode::Text:		draw_tms_text(relative_start, relative_end);			break;

								case LineMode::Refresh:		break;	/* Dealt with elsewhere. */
							}
						}

						if(end == line_buffer.next_border_column) {
							const int length = line_buffer.next_border_column - line_buffer.first_pixel_output_column;
							crt_.output_data(length * 4, size_t(length));
							pixel_origin_ = pixel_target_ = nullptr;
							asked_for_write_area_ = false;
						}
					);

					// Additional right border, if called for.
					if(line_buffer.next_border_column != 342) {
						border(line_buffer.next_border_column, 342);
					}
				}

#undef border
#undef intersect



				// -------------
				// Advance time.
				// -------------
				read_pointer_.column = end_column;
			}

			read_cycles_pool -= target_read_cycles;
			if(read_pointer_.column == 342) {
				read_pointer_.column = 0;
				read_pointer_.row = (read_pointer_.row + 1) % mode_timing_.total_lines;
			}
		}

		assert(backup.row == write_pointer_.row && backup.column == write_pointer_.column);
	}
}

void Base::output_border(int cycles, uint32_t cram_dot) {
	cycles *= 4;
	const uint32_t border_colour =
		is_sega_vdp(personality_) ?
			master_system_.colour_ram[16 + background_colour_] :
			palette[background_colour_];

	if(cram_dot) {
		uint32_t *const pixel_target = reinterpret_cast<uint32_t *>(crt_.begin_data(1));
		if(pixel_target) {
			*pixel_target = border_colour | cram_dot;
		}
		crt_.output_level(4);
		cycles -= 4;
	}

	if(cycles) {
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
}

void TMS9918::write(int address, uint8_t value) {
	// Writes to address 0 are writes to the video RAM. Store
	// the value and return.
	if(!(address & 1)) {
		write_phase_ = false;

		// Enqueue the write to occur at the next available slot.
		read_ahead_buffer_ = value;
		queued_access_ = MemoryAccess::Write;
		cycles_until_access_ = vram_access_delay();

		return;
	}

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
		if(is_sega_vdp(personality_)) {
			if(value & 0x40) {
				master_system_.cram_is_selected = true;
				return;
			}
			value &= 0xf;
		} else {
			value &= 0x7;
		}

		// This is a write to a register.
		switch(value) {
			case 0:
				if(is_sega_vdp(personality_)) {
					master_system_.vertical_scroll_lock = low_write_ & 0x80;
					master_system_.horizontal_scroll_lock = low_write_ & 0x40;
					master_system_.hide_left_column = low_write_ & 0x20;
					enable_line_interrupts_ = low_write_ & 0x10;
					master_system_.shift_sprites_8px_left = low_write_ & 0x08;
					master_system_.mode4_enable = low_write_ & 0x04;
				}
				mode2_enable_ = low_write_ & 0x02;
			break;

			case 1:
				blank_display_ = !(low_write_ & 0x40);
				generate_interrupts_ = low_write_ & 0x20;
				mode1_enable_ = low_write_ & 0x10;
				mode3_enable_ = low_write_ & 0x08;
				sprites_16x16_ = low_write_ & 0x02;
				sprites_magnified_ = low_write_ & 0x01;

				sprite_height_ = 8;
				if(sprites_16x16_) sprite_height_ <<= 1;
				if(sprites_magnified_) sprite_height_ <<= 1;
			break;

			case 2:
				pattern_name_address_ = size_t((low_write_ & 0xf) << 10) | 0x3ff;
				master_system_.pattern_name_address = pattern_name_address_ | ((personality_ == TMS::SMSVDP) ? 0x000 : 0x400);
			break;

			case 3:
				colour_table_address_ = size_t(low_write_ << 6) | 0x3f;
			break;

			case 4:
				pattern_generator_table_address_ = size_t((low_write_ & 0x07) << 11) | 0x7ff;
			break;

			case 5:
				sprite_attribute_table_address_ = size_t((low_write_ & 0x7f) << 7) | 0x7f;
				master_system_.sprite_attribute_table_address = sprite_attribute_table_address_ | ((personality_ == TMS::SMSVDP) ? 0x00 : 0x80);
			break;

			case 6:
				sprite_generator_table_address_ = size_t((low_write_ & 0x07) << 11) | 0x7ff;
				master_system_.sprite_generator_table_address = sprite_generator_table_address_ | ((personality_ == TMS::SMSVDP) ? 0x0000 : 0x1800);
			break;

			case 7:
				text_colour_ = low_write_ >> 4;
				background_colour_ = low_write_ & 0xf;
			break;

			case 8:
				if(is_sega_vdp(personality_)) {
					master_system_.horizontal_scroll = low_write_;
				}
			break;

			case 9:
				if(is_sega_vdp(personality_)) {
					master_system_.vertical_scroll = low_write_;
				}
			break;

			case 10:
				if(is_sega_vdp(personality_)) {
					line_interrupt_target = low_write_;
				}
			break;

			default:
				LOG("Unknown TMS write: " << int(low_write_) << " to " << int(value));
			break;
		}
	} else {
		// This is an access via the RAM pointer.
		if(!(value & 0x40)) {
			// A read request is enqueued upon setting the address; conversely a write
			// won't be enqueued unless and until some actual data is supplied.
			queued_access_ = MemoryAccess::Read;
			cycles_until_access_ = vram_access_delay();
		}
		master_system_.cram_is_selected = false;
	}
}

uint8_t TMS9918::get_current_line() {
	// Determine the row to return.
	constexpr int row_change_position = 63;	// This is the proper Master System value; substitute if any other VDPs turn out to have this functionality.
	int source_row =
		(write_pointer_.column < row_change_position)
			? (write_pointer_.row + mode_timing_.total_lines - 1)%mode_timing_.total_lines
			: write_pointer_.row;

	if(tv_standard_ == TVStandard::NTSC) {
		if(mode_timing_.pixel_lines == 240) {
			// NTSC 256x240:	00-FF, 00-06
		} else if(mode_timing_.pixel_lines == 224) {
			// NTSC 256x224:	00-EA, E5-FF
			if(source_row >= 0xeb) source_row -= 6;
		} else {
			// NTSC 256x192:	00-DA, D5-FF
			if(source_row >= 0xdb) source_row -= 6;
		}
	} else {
		if(mode_timing_.pixel_lines == 240) {
			// PAL 256x240:		00-FF, 00-0A, D2-FF
			if(source_row >= 267) source_row -= 0x39;
		} else if(mode_timing_.pixel_lines == 224) {
			// PAL 256x224:		00-FF, 00-02, CA-FF
			if(source_row >= 259) source_row -= 0x39;
		} else {
			// PAL 256x192:		00-F2, BA-FF
			if(source_row >= 0xf3) source_row -= 0x39;
		}
	}

	return uint8_t(source_row);
}

uint8_t TMS9918::get_latched_horizontal_counter() {
	// Translate from internal numbering, which puts pixel output
	// in the final 256 pixels of 342, to the public numbering,
	// which makes the 256 pixels the first 256 spots, but starts
	// counting at -48, and returns only the top 8 bits of the number.
	int public_counter = latched_column_ - 86;
	if(public_counter < -46) public_counter += 342;
	return uint8_t(public_counter >> 1);
}

void TMS9918::latch_horizontal_counter() {
	latched_column_ = write_pointer_.column;
}

uint8_t TMS9918::read(int address) {
	write_phase_ = false;

	// Reads from address 0 read video RAM, via the read-ahead buffer.
	if(!(address & 1)) {
		// Enqueue the write to occur at the next available slot.
		const uint8_t result = read_ahead_buffer_;
		queued_access_ = MemoryAccess::Read;
		return result;
	}

	// Reads from address 1 get the status register.
	const uint8_t result = status_;
	status_ &= ~(StatusInterrupt | StatusSpriteOverflow | StatusSpriteCollision);
	line_interrupt_pending_ = false;
	return result;
}

HalfCycles Base::half_cycles_before_internal_cycles(int internal_cycles) {
	return HalfCycles(((internal_cycles << 2) + (2 - cycles_error_)) / 3);
}

HalfCycles TMS9918::get_next_sequence_point() {
	if(!generate_interrupts_ && !enable_line_interrupts_) return HalfCycles::max();
	if(get_interrupt_line()) return HalfCycles::max();

	// Calculate the amount of time until the next end-of-frame interrupt.
	const int frame_length = 342 * mode_timing_.total_lines;
	int time_until_frame_interrupt =
		(
			((mode_timing_.end_of_frame_interrupt_position.row * 342) + mode_timing_.end_of_frame_interrupt_position.column + frame_length) -
			((write_pointer_.row * 342) + write_pointer_.column)
		) % frame_length;
	if(!time_until_frame_interrupt) time_until_frame_interrupt = frame_length;

	if(!enable_line_interrupts_) return half_cycles_before_internal_cycles(time_until_frame_interrupt);

	// Calculate when the next line interrupt will occur.
	int next_line_interrupt_row = -1;

	int cycles_to_next_interrupt_threshold = mode_timing_.line_interrupt_position - write_pointer_.column;
	int line_of_next_interrupt_threshold = write_pointer_.row;
	if(cycles_to_next_interrupt_threshold <= 0) {
		cycles_to_next_interrupt_threshold += 342;
		++line_of_next_interrupt_threshold;
	}

	if(is_sega_vdp(personality_)) {
		// If there is still time for a line interrupt this frame, that'll be it;
		// otherwise it'll be on the next frame, supposing there's ever time for
		// it at all.
		if(line_of_next_interrupt_threshold + line_interrupt_counter <= mode_timing_.pixel_lines) {
			next_line_interrupt_row = line_of_next_interrupt_threshold + line_interrupt_counter;
		} else {
			if(line_interrupt_target <= mode_timing_.pixel_lines)
				next_line_interrupt_row = mode_timing_.total_lines + line_interrupt_target;
		}
	}

	// If there's actually no interrupt upcoming, despite being enabled, either return
	// the frame end interrupt or no interrupt pending as appropriate.
	if(next_line_interrupt_row == -1) {
		return generate_interrupts_ ?
			half_cycles_before_internal_cycles(time_until_frame_interrupt) :
			HalfCycles::max();
	}

	// Figure out the number of internal cycles until the next line interrupt, which is the amount
	// of time to the next tick over and then next_line_interrupt_row - row_ lines further.
	const int local_cycles_until_line_interrupt = cycles_to_next_interrupt_threshold + (next_line_interrupt_row - line_of_next_interrupt_threshold) * 342;
	if(!generate_interrupts_) return half_cycles_before_internal_cycles(local_cycles_until_line_interrupt);

	// Return whichever interrupt is closer.
	return half_cycles_before_internal_cycles(std::min(local_cycles_until_line_interrupt, time_until_frame_interrupt));
}

HalfCycles TMS9918::get_time_until_line(int line) {
	if(line < 0) line += mode_timing_.total_lines;

	int cycles_to_next_interrupt_threshold = mode_timing_.line_interrupt_position - write_pointer_.column;
	int line_of_next_interrupt_threshold = write_pointer_.row;
	if(cycles_to_next_interrupt_threshold <= 0) {
		cycles_to_next_interrupt_threshold += 342;
		++line_of_next_interrupt_threshold;
	}

	if(line_of_next_interrupt_threshold > line) {
		line += mode_timing_.total_lines;
	}

	return half_cycles_before_internal_cycles(cycles_to_next_interrupt_threshold + (line - line_of_next_interrupt_threshold)*342);
}

bool TMS9918::get_interrupt_line() {
	return ((status_ & StatusInterrupt) && generate_interrupts_) || (enable_line_interrupts_ && line_interrupt_pending_);
}

// MARK: -

void Base::draw_tms_character(int start, int end) {
	LineBuffer &line_buffer = line_buffers_[read_pointer_.row];

	// Paint the background tiles.
	const int pixels_left = end - start;
	if(screen_mode_ == ScreenMode::MultiColour) {
		for(int c = start; c < end; ++c) {
			pixel_target_[c] = palette[
				(line_buffer.patterns[c >> 3][0] >> (((c & 4)^4))) & 15
			];
		}
	} else {
		const int shift = start & 7;
		int byte_column = start >> 3;

		int length = std::min(pixels_left, 8 - shift);

		int pattern = reverse_table.map[line_buffer.patterns[byte_column][0]] >> shift;
		uint8_t colour = line_buffer.patterns[byte_column][1];
		uint32_t colours[2] = {
			palette[(colour & 15) ? (colour & 15) : background_colour_],
			palette[(colour >> 4) ? (colour >> 4) : background_colour_]
		};

		int background_pixels_left = pixels_left;
		while(true) {
			background_pixels_left -= length;
			for(int c = 0; c < length; ++c) {
				pixel_target_[c] = colours[pattern&0x01];
				pattern >>= 1;
			}
			pixel_target_ += length;

			if(!background_pixels_left) break;
			length = std::min(8, background_pixels_left);
			byte_column++;

			pattern = reverse_table.map[line_buffer.patterns[byte_column][0]];
			colour = line_buffer.patterns[byte_column][1];
			colours[0] = palette[(colour & 15) ? (colour & 15) : background_colour_];
			colours[1] = palette[(colour >> 4) ? (colour >> 4) : background_colour_];
		}
	}

	// Paint sprites and check for collisions, but only if at least one sprite is active
	// on this line.
	if(line_buffer.active_sprite_slot) {
		const int shift_advance = sprites_magnified_ ? 1 : 2;
		// If this is the start of the line clip any part of any sprites that is off to the left.
		if(!start) {
			for(int index = 0; index < line_buffer.active_sprite_slot; ++index) {
				LineBuffer::ActiveSprite &sprite = line_buffer.active_sprites[index];
				if(sprite.x < 0) sprite.shift_position -= shift_advance * sprite.x;
			}
		}

		int sprite_buffer[256];
		int sprite_collision = 0;
		memset(&sprite_buffer[start], 0, size_t(end - start)*sizeof(sprite_buffer[0]));

		constexpr uint32_t sprite_colour_selection_masks[2] = {0x00000000, 0xffffffff};
		constexpr int colour_masks[16] = {0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

		// Draw all sprites into the sprite buffer.
		const int shifter_target = sprites_16x16_ ? 32 : 16;
		for(int index = line_buffer.active_sprite_slot - 1; index >= 0; --index) {
			LineBuffer::ActiveSprite &sprite = line_buffer.active_sprites[index];
			if(sprite.shift_position < shifter_target) {
				const int pixel_start = std::max(start, sprite.x);
				for(int c = pixel_start; c < end && sprite.shift_position < shifter_target; ++c) {
					const int shift = (sprite.shift_position >> 1) ^ 7;
					int sprite_colour = (sprite.image[shift >> 3] >> (shift & 7)) & 1;

					// A colision is detected regardless of sprite colour ...
					sprite_collision |= sprite_buffer[c] & sprite_colour;
					sprite_buffer[c] |= sprite_colour;

					// ... but a sprite with the transparent colour won't actually be visible.
					sprite_colour &= colour_masks[sprite.image[2]&15];
					pixel_origin_[c] =
						(pixel_origin_[c] & sprite_colour_selection_masks[sprite_colour^1]) |
						(palette[sprite.image[2]&15] & sprite_colour_selection_masks[sprite_colour]);

					sprite.shift_position += shift_advance;
				}
			}
		}

		status_ |= sprite_collision << StatusSpriteCollisionShift;
	}
}

void Base::draw_tms_text(int start, int end) {
	LineBuffer &line_buffer = line_buffers_[read_pointer_.row];
	const uint32_t colours[2] = { palette[background_colour_], palette[text_colour_] };

	const int shift = start % 6;
	int byte_column = start / 6;
	int pattern = reverse_table.map[line_buffer.patterns[byte_column][0]] >> shift;
	int pixels_left = end - start;
	int length = std::min(pixels_left, 6 - shift);
	while(true) {
		pixels_left -= length;
		for(int c = 0; c < length; ++c) {
			pixel_target_[c] = colours[pattern&0x01];
			pattern >>= 1;
		}
		pixel_target_ += length;

		if(!pixels_left) break;
		length = std::min(6, pixels_left);
		byte_column++;
		pattern = reverse_table.map[line_buffer.patterns[byte_column][0]];
	}
}

void Base::draw_sms(int start, int end, uint32_t cram_dot) {
	LineBuffer &line_buffer = line_buffers_[read_pointer_.row];
	int colour_buffer[256];

	/*
		Add extra border for any pixels that fall before the fine scroll.
	*/
	int tile_start = start, tile_end = end;
	int tile_offset = start;
	if(read_pointer_.row >= 16 || !master_system_.horizontal_scroll_lock) {
		for(int c = start; c < (line_buffer.latched_horizontal_scroll & 7); ++c) {
			colour_buffer[c] = 16 + background_colour_;
			++tile_offset;
		}

		// Remove the border area from that to which tiles will be drawn.
		tile_start = std::max(start - (line_buffer.latched_horizontal_scroll & 7), 0);
		tile_end = std::max(end - (line_buffer.latched_horizontal_scroll & 7), 0);
	}


	uint32_t pattern;
	uint8_t *const pattern_index = reinterpret_cast<uint8_t *>(&pattern);

	/*
		Add background tiles; these will fill the colour_buffer with values in which
		the low five bits are a palette index, and bit six is set if this tile has
		priority over sprites.
	*/
	if(tile_start < end) {
		const int shift = tile_start & 7;
		int byte_column = tile_start >> 3;
		int pixels_left = tile_end - tile_start;
		int length = std::min(pixels_left, 8 - shift);

		pattern = *reinterpret_cast<const uint32_t *>(line_buffer.patterns[byte_column]);
		if(line_buffer.names[byte_column].flags&2)
			pattern >>= shift;
		else
			pattern <<= shift;

		while(true) {
			const int palette_offset = (line_buffer.names[byte_column].flags&0x18) << 1;
			if(line_buffer.names[byte_column].flags&2) {
				for(int c = 0; c < length; ++c) {
					colour_buffer[tile_offset] =
						((pattern_index[3] & 0x01) << 3) |
						((pattern_index[2] & 0x01) << 2) |
						((pattern_index[1] & 0x01) << 1) |
						((pattern_index[0] & 0x01) << 0) |
						palette_offset;
					++tile_offset;
					pattern >>= 1;
				}
			} else {
				for(int c = 0; c < length; ++c) {
					colour_buffer[tile_offset] =
						((pattern_index[3] & 0x80) >> 4) |
						((pattern_index[2] & 0x80) >> 5) |
						((pattern_index[1] & 0x80) >> 6) |
						((pattern_index[0] & 0x80) >> 7) |
						palette_offset;
					++tile_offset;
					pattern <<= 1;
				}
			}

			pixels_left -= length;
			if(!pixels_left) break;

			length = std::min(8, pixels_left);
			byte_column++;
			pattern = *reinterpret_cast<const uint32_t *>(line_buffer.patterns[byte_column]);
		}
	}

	/*
		Apply sprites (if any).
	*/
	if(line_buffer.active_sprite_slot) {
		const int shift_advance = sprites_magnified_ ? 1 : 2;

		// If this is the start of the line clip any part of any sprites that is off to the left.
		if(!start) {
			for(int index = 0; index < line_buffer.active_sprite_slot; ++index) {
				LineBuffer::ActiveSprite &sprite = line_buffer.active_sprites[index];
				if(sprite.x < 0) sprite.shift_position -= shift_advance * sprite.x;
			}
		}

		int sprite_buffer[256];
		int sprite_collision = 0;
		memset(&sprite_buffer[start], 0, size_t(end - start)*sizeof(sprite_buffer[0]));

		// Draw all sprites into the sprite buffer.
		for(int index = line_buffer.active_sprite_slot - 1; index >= 0; --index) {
			LineBuffer::ActiveSprite &sprite = line_buffer.active_sprites[index];
			if(sprite.shift_position < 16) {
				const int pixel_start = std::max(start, sprite.x);

				// TODO: it feels like the work below should be simplifiable;
				// the double shift in particular, and hopefully the variable shift.
				for(int c = pixel_start; c < end && sprite.shift_position < 16; ++c) {
					const int shift = (sprite.shift_position >> 1);
					const int sprite_colour =
						(((sprite.image[3] << shift) & 0x80) >> 4) |
						(((sprite.image[2] << shift) & 0x80) >> 5) |
						(((sprite.image[1] << shift) & 0x80) >> 6) |
						(((sprite.image[0] << shift) & 0x80) >> 7);

					if(sprite_colour) {
						sprite_collision |= sprite_buffer[c];
						sprite_buffer[c] = sprite_colour | 0x10;
					}

					sprite.shift_position += shift_advance;
				}
			}
		}

		// Draw the sprite buffer onto the colour buffer, wherever the tile map doesn't have
		// priority (or is transparent).
		for(int c = start; c < end; ++c) {
			if(
				sprite_buffer[c] &&
				(!(colour_buffer[c]&0x20) || !(colour_buffer[c]&0xf))
			) colour_buffer[c] = sprite_buffer[c];
		}

		if(sprite_collision)
			status_ |= StatusSpriteCollision;
	}

	// Map from the 32-colour buffer to real output pixels, applying the specific CRAM dot if any.
	pixel_target_[start] = master_system_.colour_ram[colour_buffer[start] & 0x1f] | cram_dot;
	for(int c = start+1; c < end; ++c) {
		pixel_target_[c] = master_system_.colour_ram[colour_buffer[c] & 0x1f];
	}

	// If the VDP is set to hide the left column and this is the final call that'll come
	// this line, hide it.
	if(end == 256) {
		if(master_system_.hide_left_column) {
			pixel_origin_[0] = pixel_origin_[1] = pixel_origin_[2] = pixel_origin_[3] =
			pixel_origin_[4] = pixel_origin_[5] = pixel_origin_[6] = pixel_origin_[7] =
				master_system_.colour_ram[16 + background_colour_];
		}
	}
}
