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

using namespace TI::TMS;

namespace {

const uint8_t StatusInterrupt = 0x80;
const uint8_t StatusSpriteOverflow = 0x40;

const int StatusSpriteCollisionShift = 5;
const uint8_t StatusSpriteCollision = 0x20;

struct ReverseTable {
	std::uint8_t map[256];

	ReverseTable() {
		for(int c = 0; c < 256; ++c) {
			map[c] = static_cast<uint8_t>(
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
	}
} reverse_table;

}

Base::Base(Personality p) :
	personality_(p),
	// 342 internal cycles are 228/227.5ths of a line, so 341.25 cycles should be a whole
	// line. Therefore multiply everything by four, but set line length to 1365 rather than 342*4 = 1368.
	crt_(new Outputs::CRT::CRT(1365, 4, Outputs::CRT::DisplayType::NTSC60, 4)) {

	switch(p) {
		case TI::TMS::TMS9918A:
		case TI::TMS::SMSVDP:
		case TI::TMS::GGVDP:
			ram_.resize(16 * 1024);
		break;
		case TI::TMS::V9938:
			ram_.resize(128 * 1024);
		break;
		case TI::TMS::V9958:
			ram_.resize(192 * 1024);
		break;
	}

	if(is_sega_vdp(personality_)) {
		mode_timing_.line_interrupt_position = 15;
	}
}

TMS9918::TMS9918(Personality p):
 	Base(p) {
	// Unimaginatively, this class just passes RGB through to the shader. Investigation is needed
	// into whether there's a more natural form.
	crt_->set_rgb_sampling_function(
		"vec3 rgb_sample(usampler2D sampler, vec2 coordinate)"
		"{"
			"return texture(sampler, coordinate).rgb / vec3(255.0);"
		"}");
	crt_->set_video_signal(Outputs::CRT::VideoSignal::RGB);
	crt_->set_visible_area(Outputs::CRT::Rect(0.055f, 0.025f, 0.9f, 0.9f));
	crt_->set_input_gamma(2.8f);

	// The TMS remains in-phase with the NTSC colour clock; this is an empirical measurement
	// intended to produce the correct relationship between the hard edges between pixels and
	// the colour clock. It was eyeballed rather than derived from any knowledge of the TMS
	// colour burst generator because I've yet to find any.
	crt_->set_immediate_default_phase(0.85f);
}

Outputs::CRT::CRT *TMS9918::get_crt() {
	return crt_.get();
}

void Base::reset_sprite_collection() {
	sprite_set_.sprites_stopped = false;
	sprite_set_.fetched_sprite_slot = sprite_set_.active_sprite_slot;
	sprite_set_.active_sprite_slot = 0;

	for(int c = 0; c < sprite_set_.fetched_sprite_slot; ++c) {
		sprite_set_.active_sprites[c].shift_position = 0;
	}
}

void Base::posit_sprite(int sprite_number, int sprite_position, int screen_row) {
	if(!(status_ & StatusSpriteOverflow)) {
		status_ = static_cast<uint8_t>((status_ & ~31) | sprite_number);
	}
	if(sprite_set_.sprites_stopped)
		return;

//	const int sprite_position = ram_[sprite_attribute_table_address_ + static_cast<size_t>(sprite_number << 2)];
	// A sprite Y of 208 means "don't scan the list any further".
	if(mode_timing_.allow_sprite_terminator && sprite_position == 208) {
		sprite_set_.sprites_stopped = true;
		return;
	}

	const int sprite_row = (screen_row - sprite_position)&255;
	if(sprite_row < 0 || sprite_row >= sprite_height_) return;

	if(sprite_set_.active_sprite_slot == mode_timing_.maximum_visible_sprites) {
		status_ |= StatusSpriteOverflow;
		return;
	}

	SpriteSet::ActiveSprite &sprite = sprite_set_.active_sprites[sprite_set_.active_sprite_slot];
	sprite.index = sprite_number;
	sprite.row = sprite_row >> (sprites_magnified_ ? 1 : 0);
	++sprite_set_.active_sprite_slot;
}

void Base::get_sprite_contents(int field, int cycles_left, int screen_row) {
/*	int sprite_id = field / 6;
	field %= 6;

	while(true) {
		const int cycles_in_sprite = std::min(cycles_left, 6 - field);
		cycles_left -= cycles_in_sprite;
		const int final_field = cycles_in_sprite + field;

		assert(sprite_id < 4);
		SpriteSet::ActiveSprite &sprite = sprite_sets_[active_sprite_set_].active_sprites[sprite_id];

		if(field < 4) {
			std::memcpy(
				&sprite.info[field],
				&ram_[sprite_attribute_table_address_ + static_cast<size_t>((sprite.index << 2) + field)],
				static_cast<size_t>(std::min(4, final_field) - field));
		}

		field = std::min(4, final_field);
		const int sprite_offset = sprite.info[2] & ~(sprites_16x16_ ? 3 : 0);
		const size_t sprite_address = sprite_generator_table_address_ + static_cast<size_t>(sprite_offset << 3) + sprite.row; // TODO: recalclate sprite.row from screen_row (?)
		while(field < final_field) {
			sprite.image[field - 4] = ram_[sprite_address + static_cast<size_t>(((field - 4) << 4))];
			field++;
		}

		if(!cycles_left) return;
		field = 0;
		sprite_id++;
	}*/
}

void TMS9918::run_for(const HalfCycles cycles) {
	// As specific as I've been able to get:
	// Scanline time is always 228 cycles.
	// PAL output is 313 lines total. NTSC output is 262 lines total.
	// Interrupt is signalled upon entering the lower border.

	// Keep a count of cycles separate from internal counts to avoid
	// potential errors mapping back and forth.
	half_cycles_into_frame_ = (half_cycles_into_frame_ + cycles) % HalfCycles(mode_timing_.total_lines * 228 * 2);

	// Convert 456 clocked half cycles per line to 342 internal cycles per line;
	// the internal clock is 1.5 times the nominal 3.579545 Mhz that I've advertised
	// for this part. So multiply by three quarters.
	int int_cycles = (cycles.as_int() * 3) + cycles_error_;
	cycles_error_ = int_cycles & 3;
	int_cycles >>= 2;
	if(!int_cycles) return;

	while(int_cycles) {
		// Determine how much time has passed in the remainder of this line, and proceed.
		const int cycles_left = std::min(342 - column_, int_cycles);
		const int end_column = column_ + cycles_left;


		// ------------------------
		// Perform memory accesses.
		// ------------------------
#define fetch(function)	\
	if(end_column < 171) {	\
		function<true>(first_window, final_window);\
	} else {\
		function<false>(first_window, final_window);\
	}

		// column_ and end_column are in 342-per-line cycles;
		// adjust them to a count of windows.
		const int first_window = column_ >> 1;
		const int final_window = end_column >> 1;
		if(first_window != final_window) {
			switch(line_mode_) {
				case LineMode::Text:		fetch(fetch_tms_text);		break;
				case LineMode::Character:	fetch(fetch_tms_character);	break;
				case LineMode::SMS:			fetch(fetch_sms);			break;
				case LineMode::Refresh:		fetch(fetch_tms_refresh);	break;
			}
		}

#undef fetch


		// --------------------
		// Output video stream.
		// --------------------

#define intersect(left, right, code)	\
	{	\
		const int start = std::max(column_, left);	\
		const int end = std::min(end_column, right);	\
		if(end > start) {\
			code;\
		}\
	}

		if(line_mode_ == LineMode::Refresh || row_ > mode_timing_.pixel_lines) {
			if(row_ >= mode_timing_.first_vsync_line && row_ < mode_timing_.first_vsync_line+4) {
				// Vertical sync.
				if(end_column == 342) {
					crt_->output_sync(342 * 4);
				}
			} else {
				// Right border.
				intersect(0, 15, output_border(end - start));

				// Blanking region.
				if(column_ < 73 && end_column >= 73) {
					crt_->output_blank(8*4);
					crt_->output_sync(26*4);
					crt_->output_blank(2*4);
					crt_->output_default_colour_burst(14*4);
					crt_->output_blank(8*4);
				}

				// Most of line.
				intersect(73, 342, output_border(end - start));
			}
		} else {
			// Right border.
			intersect(0, 15, output_border(end - start));

			// Blanking region.
			if(column_ < 73 && end_column >= 73) {
				crt_->output_blank(8*4);
				crt_->output_sync(26*4);
				crt_->output_blank(2*4);
				crt_->output_default_colour_burst(14*4);
				crt_->output_blank(8*4);
			}

			// Left border.
			intersect(73, mode_timing_.first_pixel_output_column, output_border(end - start));

			// Pixel region.
			intersect(
				mode_timing_.first_pixel_output_column,
				mode_timing_.next_border_column,
				if(start == mode_timing_.first_pixel_output_column) {
					pixel_origin_ = pixel_target_ = reinterpret_cast<uint32_t *>(
						crt_->allocate_write_area(static_cast<unsigned int>(mode_timing_.next_border_column - mode_timing_.first_pixel_output_column) + 8)	// TODO: the +8 is really for the SMS only; make it optional.
					);
				}

				if(pixel_target_) {
					const int relative_start = start - mode_timing_.first_pixel_output_column;
					const int relative_end = end - mode_timing_.first_pixel_output_column;
					switch(line_mode_) {
						case LineMode::SMS:			draw_sms(relative_start, relative_end);					break;
						case LineMode::Character:	draw_tms_character(relative_start, relative_end);		break;
						case LineMode::Text:		draw_tms_text(relative_start, relative_end);			break;

						case LineMode::Refresh:		break;	/* Dealt with elsewhere. */
					}
				}

				if(end == mode_timing_.next_border_column) {
					const unsigned int length = static_cast<unsigned int>(mode_timing_.next_border_column - mode_timing_.first_pixel_output_column);
					crt_->output_data(length * 4, length);
					pixel_origin_ = pixel_target_ = nullptr;
				}
			);

			// Additional right border, if called for.
			if(mode_timing_.next_border_column != 342) {
				intersect(mode_timing_.next_border_column, 342, output_border(end - start));
			}
		}

#undef intersect

		// -----------------
		// End video stream.
		// -----------------


/*

			// --------------
			// Output pixels.
			// --------------


						case LineMode::Character: {
							// If this is the start of the visible area, seed sprite shifter positions.
							SpriteSet &sprite_set = sprite_sets_[active_sprite_set_ ^ 1];
							if(output_column_ == first_pixel_column_) {
								int c = sprite_set.active_sprite_slot;
								while(c--) {
									SpriteSet::ActiveSprite &sprite = sprite_set.active_sprites[c];
									sprite.shift_position = -sprite.info[1];
									if(sprite.info[3] & 0x80) {
										sprite.shift_position += 32;
										if(sprite.shift_position > 0 && !sprites_magnified_)
											sprite.shift_position *= 2;
									}
								}
							}

							// Paint the background tiles.
							const int pixels_left = pixels_end - output_column_;
							if(current_mode_ == ScreenMode::MultiColour) {
								int pixel_location = output_column_ - first_pixel_column_;
								for(int c = 0; c < pixels_left; ++c) {
									pixel_target_[c] = palette[
										(pattern_buffer_[(pixel_location + c) >> 3] >> (((pixel_location + c) & 4)^4)) & 15
									];
								}
								pixel_target_ += pixels_left;
							} else {
								const int shift = (output_column_ - first_pixel_column_) & 7;
								int byte_column = (output_column_ - first_pixel_column_) >> 3;

								int length = std::min(pixels_left, 8 - shift);

								int pattern = reverse_table.map[pattern_buffer_[byte_column]] >> shift;
								uint8_t colour = colour_buffer_[byte_column];
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

									pattern = reverse_table.map[pattern_buffer_[byte_column]];
									colour = colour_buffer_[byte_column];
									colours[0] = palette[(colour & 15) ? (colour & 15) : background_colour_];
									colours[1] = palette[(colour >> 4) ? (colour >> 4) : background_colour_];
								}
							}

							// Paint sprites and check for collisions, but only if at least one sprite is active
							// on this line.
							if(sprite_set.active_sprite_slot) {
								int sprite_pixels_left = pixels_left;
								const int shift_advance = sprites_magnified_ ? 1 : 2;

								static const uint32_t sprite_colour_selection_masks[2] = {0x00000000, 0xffffffff};
								static const int colour_masks[16] = {0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

								while(sprite_pixels_left--) {
									// sprite_colour is the colour that's going to reach the display after sprite logic has been
									// applied; by default assume that nothing is going to be drawn.
									uint32_t sprite_colour = pixel_base_[output_column_ - first_pixel_column_];

									// The sprite_mask is used to keep track of whether two sprites have both sought to output
									// a pixel at the same location, and to feed that into the status register's sprite
									// collision bit.
									int sprite_mask = 0;

									int c = sprite_set.active_sprite_slot;
									while(c--) {
										SpriteSet::ActiveSprite &sprite = sprite_set.active_sprites[c];

										if(sprite.shift_position < 0) {
											sprite.shift_position++;
											continue;
										} else if(sprite.shift_position < 32) {
											int mask = sprite.image[sprite.shift_position >> 4] << ((sprite.shift_position&15) >> 1);
											mask = (mask >> 7) & 1;

											// Ignore the right half of whatever was collected if sprites are not in 16x16 mode.
											if(sprite.shift_position < (sprites_16x16_ ? 32 : 16)) {
												// If any previous sprite has been painted in this column and this sprite
												// has this pixel set, set the sprite collision status bit.
												status_ |= (mask & sprite_mask) << StatusSpriteCollisionShift;
												sprite_mask |= mask;

												// Check that the sprite colour is not transparent
												mask &= colour_masks[sprite.info[3]&15];
												sprite_colour = (sprite_colour & sprite_colour_selection_masks[mask^1]) | (palette[sprite.info[3]&15] & sprite_colour_selection_masks[mask]);
											}

											sprite.shift_position += shift_advance;
										}
									}

									// Output whichever sprite colour was on top.
									pixel_base_[output_column_ - first_pixel_column_] = sprite_colour;
									output_column_++;
								}
							}

							output_column_ = pixels_end;
						} break;
					}


 		}*/


		if(column_ < mode_timing_.line_interrupt_position && end_column >= mode_timing_.line_interrupt_position) {
			if(row_ <= mode_timing_.pixel_lines) {
				--line_interrupt_counter;
				if(line_interrupt_counter == 0xff) {
//					line_interrupt_pending_ = true;
					line_interrupt_counter = line_interrupt_target;
				}
			} else {
				line_interrupt_counter = line_interrupt_target;
			}
		}


		// -------------
		// Advance time.
		// -------------
		column_ = end_column;		// column_ is now the column that has been reached in this line.
		int_cycles -= cycles_left;	// Count down duration to run for.



		// -----------------------------------
		// Prepare for next line, potentially.
		// -----------------------------------
		if(column_ == 342) {
			column_ = 0;
			row_ = (row_ + 1) % mode_timing_.total_lines;
			if(row_ == mode_timing_.pixel_lines) status_ |= StatusInterrupt;

			// Establish the output mode for the next line.
			set_current_mode();

			// Based on the output mode, pick a line mode.
			mode_timing_.first_pixel_output_column = 86;
			mode_timing_.next_border_column = 342;
			mode_timing_.maximum_visible_sprites = 4;
			switch(screen_mode_) {
				case ScreenMode::Text:
					line_mode_ = LineMode::Text;
					mode_timing_.first_pixel_output_column = 94;
					mode_timing_.next_border_column = 334;
				break;
				case ScreenMode::SMSMode4:
					line_mode_ = LineMode::SMS;
					mode_timing_.maximum_visible_sprites = 8;
				break;
				default:
					line_mode_ = LineMode::Character;
				break;
			}

			if((screen_mode_ == ScreenMode::Blank) || (row_ >= mode_timing_.pixel_lines && row_ != mode_timing_.total_lines-1)) line_mode_ = LineMode::Refresh;
		}
	}
}

void Base::output_border(int cycles) {
	uint32_t *const pixel_target = reinterpret_cast<uint32_t *>(crt_->allocate_write_area(1));
	if(pixel_target) {
		if(is_sega_vdp(personality_)) {
			*pixel_target = master_system_.colour_ram[16 + background_colour_];
		} else {
			*pixel_target = palette[background_colour_];
		}
	}
	crt_->output_level(static_cast<unsigned int>(cycles) * 4);
}

void TMS9918::set_register(int address, uint8_t value) {
	// Writes to address 0 are writes to the video RAM. Store
	// the value and return.
	if(!(address & 1)) {
		write_phase_ = false;

		// Enqueue the write to occur at the next available slot.
		read_ahead_buffer_ = value;
		queued_access_ = MemoryAccess::Write;

		return;
	}

	// Writes to address 1 are performed in pairs; if this is the
	// low byte of a value, store it and wait for the high byte.
	if(!write_phase_) {
		low_write_ = value;
		write_phase_ = true;
		return;
	}

	write_phase_ = false;
	if(value & 0x80) {
		switch(personality_) {
			default:
				value &= 0x7;
			break;
			case TI::TMS::SMSVDP:
				if(value & 0x40) {
					ram_pointer_ = static_cast<uint16_t>(low_write_ | (value << 8));
					master_system_.cram_is_selected = true;
					return;
				}
				value &= 0xf;
			break;
		}

		// This is a write to a register.
		switch(value) {
			case 0:
				if(is_sega_vdp(personality_)) {
					master_system_.vertical_scroll_lock = !!(low_write_ & 0x80);
					master_system_.horizontal_scroll_lock = !!(low_write_ & 0x40);
					master_system_.hide_left_column = !!(low_write_ & 0x20);
					enable_line_interrupts_ = !!(low_write_ & 0x10);
					master_system_.shift_sprites_8px_left = !!(low_write_ & 0x08);
					master_system_.mode4_enable = !!(low_write_ & 0x04);
				}
				mode2_enable_ = !!(low_write_ & 0x02);
			break;

			case 1:
				blank_display_ = !(low_write_ & 0x40);
				generate_interrupts_ = !!(low_write_ & 0x20);
				mode1_enable_ = !!(low_write_ & 0x10);
				mode3_enable_ = !!(low_write_ & 0x08);
				sprites_16x16_ = !!(low_write_ & 0x02);
				sprites_magnified_ = !!(low_write_ & 0x01);

				sprite_height_ = 8;
				if(sprites_16x16_) sprite_height_ <<= 1;
				if(sprites_magnified_) sprite_height_ <<= 1;
			break;

			case 2:
				pattern_name_address_ = size_t((low_write_ & 0xf) << 10) | 0x3ff;
			break;

			case 3:
				colour_table_address_ = size_t(low_write_ << 6) | 0x1f;
			break;

			case 4:
				pattern_generator_table_address_ = size_t((low_write_ & 0x07) << 11) | 0x7ff;
			break;

			case 5:
				sprite_attribute_table_address_ = size_t((low_write_ & 0x7f) << 7) | 0x7f;
			break;

			case 6:
				sprite_generator_table_address_ = size_t((low_write_ & 0x07) << 11) | 0x7ff;
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
					line_interrupt_target = value;
				}
			break;

			default:
				printf("Unknown TMS write: %d to %d\n", low_write_, value);
			break;
		}
	} else {
		// This is an access via the RAM pointer.
		ram_pointer_ = static_cast<uint16_t>(low_write_ | (value << 8));
		if(!(value & 0x40)) {
			// A read request is enqueued upon setting the address; conversely a write
			// won't be enqueued unless and until some actual data is supplied.
			queued_access_ = MemoryAccess::Read;
		}
		master_system_.cram_is_selected = false;
	}
}

uint8_t TMS9918::get_current_line() {
	return static_cast<uint8_t>(row_);
}

uint8_t TMS9918::get_register(int address) {
	write_phase_ = false;

	// Reads from address 0 read video RAM, via the read-ahead buffer.
	if(!(address & 1)) {
		// Enqueue the write to occur at the next available slot.
		uint8_t result = read_ahead_buffer_;
		queued_access_ = MemoryAccess::Read;
		return result;
	}

	// Reads from address 1 get the status register.
	uint8_t result = status_;
	status_ &= ~(StatusInterrupt | StatusSpriteOverflow | StatusSpriteCollision);
	line_interrupt_pending_ = false;
	return result;
}

 HalfCycles TMS9918::get_time_until_interrupt() {
	if(!generate_interrupts_ && !enable_line_interrupts_) return HalfCycles(-1);
	if(get_interrupt_line()) return HalfCycles(0);

	// Calculate the amount of time until the next end-of-frame interrupt.
	const int half_cycles_per_frame = mode_timing_.total_lines * 228 * 2;
	const int half_cycles_remaining = (192 * 228 * 2 + half_cycles_per_frame - half_cycles_into_frame_.as_int()) % half_cycles_per_frame;
	const auto time_until_frame_interrupt = HalfCycles(half_cycles_remaining ? half_cycles_remaining : half_cycles_per_frame);

	// Calculate the number of times the line interrupt position will be decremented this frame.
//	return HalfCycles(20);
/*	auto time_until_line_count = mode_timing_.line_interrupt_position - row_;
	auto decrements_left_this_frame = mode_timing_.pixel_lines - row_;
	if(time_until_line_count > 0) {
		++decrements_left_this_frame;
	}

	// If that's enough to underflow the line counter, there's the next interupt.
	HalfCycles time_until_line_interrupt;
	if(decrements_left_this_frame >= line_interrupt_counter+1) {
		time_until_line_interrupt = HalfCycles
	}

	if(!enable_line_interrupts_) {
		return time_until_frame_interrupt;
	} else if(!generate_interrupts_) {

	}*/

	return time_until_frame_interrupt;
}

bool TMS9918::get_interrupt_line() {
	return ((status_ & StatusInterrupt) && generate_interrupts_) || (enable_line_interrupts_ && line_interrupt_pending_);
}

// MARK: -

void Base::draw_tms_character(int start, int end) {
//	if(!start) printf("\n");
//	printf("%d to %d | ", start, end);
//	for(int c = start; c < end; ++c) {
//		pixel_target_[c] = static_cast<uint32_t>(c * 0x01010101);
//	}
}

void Base::draw_tms_text(int start, int end) {
	const uint32_t colours[2] = { palette[background_colour_], palette[text_colour_] };

	const int shift = start % 6;
	int byte_column = start / 6;
	int pattern = reverse_table.map[pattern_buffer_[byte_column]] >> shift;
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
		pattern = reverse_table.map[pattern_buffer_[byte_column]];
	}
}

void Base::draw_sms(int start, int end) {
	int colour_buffer[256];

	/*
		Add extra border for any pixels that fall before the fine scroll.
	*/
	int tile_start = start, tile_end = end;
	if(row_ >= 16 || !master_system_.horizontal_scroll_lock) {
		for(int c = start; c < (master_system_.horizontal_scroll & 7); ++c) {
			colour_buffer[c] = 16 + background_colour_;
		}

		// Remove the border area from that to which tiles will be drawn.
		tile_start = std::max(start - (master_system_.horizontal_scroll & 7), 0);
		tile_end = std::max(end - (master_system_.horizontal_scroll & 7), 0);
	}


	uint32_t pattern;
	uint8_t *const pattern_index = reinterpret_cast<uint8_t *>(&pattern);

	/*
		Add background tiles; these will fill the colour_buffer with values in which
		the low five bits are a palette index, and bit six is set if this tile has
		priority over sprites.
	*/
	if(tile_start < end) {
		int offset = (master_system_.horizontal_scroll & 7) + tile_start;
		const int shift = tile_start & 7;
		int byte_column = tile_start >> 3;
		int pixels_left = tile_end - tile_start;
		int length = std::min(pixels_left, 8 - shift);

		pattern = *reinterpret_cast<uint32_t *>(master_system_.tile_graphics[byte_column]);
		if(master_system_.names[byte_column].flags&2)
			pattern >>= shift;
		else
			pattern <<= shift;

		while(true) {
			pixels_left -= length;
			const int palette_offset = (master_system_.names[byte_column].flags&0x18) << 1;
			if(master_system_.names[byte_column].flags&2) {
				for(int c = 0; c < length; ++c) {
					colour_buffer[offset] =
						((pattern_index[3] & 0x01) << 3) |
						((pattern_index[2] & 0x01) << 2) |
						((pattern_index[1] & 0x01) << 1) |
						((pattern_index[0] & 0x01) << 0) |
						palette_offset;
					++offset;
					pattern >>= 1;
				}
			} else {
				for(int c = 0; c < length; ++c) {
					colour_buffer[offset] =
						((pattern_index[3] & 0x80) >> 4) |
						((pattern_index[2] & 0x80) >> 5) |
						((pattern_index[1] & 0x80) >> 6) |
						((pattern_index[0] & 0x80) >> 7) |
						palette_offset;
					++offset;
					pattern <<= 1;
				}
			}

			if(!pixels_left) break;
			length = std::min(8, pixels_left);
			byte_column++;
			pattern = *reinterpret_cast<uint32_t *>(master_system_.tile_graphics[byte_column]);
		}
	}

	/*
		Apply sprites (if any).
	*/
	if(sprite_set_.fetched_sprite_slot) {
		int sprite_buffer[256];
		int sprite_collision = 0;
		memset(&sprite_buffer[start], 0, size_t(end - start)*sizeof(int));

		// Draw all sprites into the sprite buffer.
		for(int index = sprite_set_.fetched_sprite_slot - 1; index >= 0; --index) {
			SpriteSet::ActiveSprite &sprite = sprite_set_.active_sprites[index];
			if(sprite.shift_position < 16) {
				int pixel_start = std::max(start, sprite.x);

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

					sprite.shift_position += sprites_magnified_ ? 1 : 2;
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

		if(sprite_collision) status_ |= StatusSpriteCollision;
	}

	// Map from the 32-colour buffer to real output pixels.
	for(int c = start; c < end; ++c) {
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
