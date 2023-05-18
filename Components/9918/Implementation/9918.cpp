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
		// Cf. https://www.smspower.org/forums/8161-SMSDisplayTiming

		// "For a line interrupt, /INT is pulled low 608 mclks into the appropriate scanline relative to pixel 0.
		// This is 3 mclks before the rising edge of /HSYNC which starts the next scanline."
		//
		// i.e. it's 304 internal clocks after the end of the left border.
		mode_timing_.line_interrupt_position = (LineLayout<personality>::EndOfLeftBorder + 304) % LineLayout<personality>::CyclesPerLine;

		// For a frame interrupt, /INT is pulled low 607 mclks into scanline 192 (of scanlines 0 through 261) relative to pixel 0.
		// This is 4 mclks before the rising edge of /HSYNC which starts the next scanline.
		//
		// i.e. it's 1/2 cycle before the line interrupt position, which I have rounded. Ugh.
		mode_timing_.end_of_frame_interrupt_position.column = mode_timing_.line_interrupt_position - 1;
		mode_timing_.end_of_frame_interrupt_position.row = 192 + (LineLayout<personality>::EndOfLeftBorder + 304) / LineLayout<personality>::CyclesPerLine;
	}

	if constexpr (is_yamaha_vdp(personality)) {
		// TODO: this is used for interrupt _prediction_ but won't handle text modes correctly, and indeed
		// can't be just a single value where the programmer has changed into or out of text modes during the
		// middle of a line, since screen mode is latched (so it'll be one value for that line, another from then onwards).a
		mode_timing_.line_interrupt_position = LineLayout<personality>::EndOfPixels;
	}

	// Establish that output is delayed after reading by `output_lag` cycles,
	// i.e. the fetch pointer is currently _ahead_ of the output pointer.
	output_pointer_.row = output_pointer_.column = 0;

	fetch_pointer_ = output_pointer_;
	fetch_pointer_.column += output_lag;

	fetch_line_buffer_ = line_buffers_.begin();
	draw_line_buffer_ = line_buffers_.begin();
	fetch_sprite_buffer_ = sprite_buffers_.begin();
}

template <Personality personality>
TMS9918<personality>::TMS9918() {
	this->crt_.set_display_type(Outputs::Display::DisplayType::RGB);

	if constexpr (is_yamaha_vdp(personality)) {
		this->crt_.set_visible_area(Outputs::Display::Rect(0.07f, 0.065f, 0.875f, 0.875f));
	} else {
		this->crt_.set_visible_area(Outputs::Display::Rect(0.07f, 0.0375f, 0.875f, 0.875f));
	}

	// The TMS remains in-phase with the NTSC colour clock; this is an empirical measurement
	// intended to produce the correct relationship between the hard edges between pixels and
	// the colour clock. It was eyeballed rather than derived from any knowledge of the TMS
	// colour burst generator because I've yet to find any.
	this->crt_.set_immediate_default_phase(0.85f);
}

template <Personality personality>
void TMS9918<personality>::set_tv_standard(TVStandard standard) {
	// TODO: the Yamaha is programmable on this at runtime.
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

void SpriteBuffer::reset_sprite_collection() {
	sprites_stopped = false;
	active_sprite_slot = 0;

	for(int c = 0; c < 8; ++c) {
		active_sprites[c].shift_position = 0;
	}
}

template <Personality personality>
void Base<personality>::posit_sprite(int sprite_number, int sprite_position, uint8_t screen_row) {
	// Evaluation of visibility of sprite 0 is always the first step in
	// populating a sprite buffer; so use it to uncork a new one.
	if(!sprite_number) {
		advance(fetch_sprite_buffer_);
		fetched_sprites_ = &*fetch_sprite_buffer_;
		fetch_sprite_buffer_->reset_sprite_collection();
		fetch_sprite_buffer_->sprite_terminator = mode_timing_.sprite_terminator(fetch_line_buffer_->screen_mode);

		if constexpr (SpriteBuffer::test_is_filling) {
			fetch_sprite_buffer_->is_filling = true;
		}
	}

	if(!(status_ & StatusSpriteOverflow)) {
		status_ = uint8_t((status_ & ~0x1f) | (sprite_number & 0x1f));
	}
	if(fetch_sprite_buffer_->sprites_stopped) return;

	// A sprite Y of 208 means "don't scan the list any further".
	if(mode_timing_.allow_sprite_terminator && sprite_position == fetch_sprite_buffer_->sprite_terminator) {
		fetch_sprite_buffer_->sprites_stopped = true;
		return;
	}

	const auto sprite_row = uint8_t(screen_row - sprite_position);
	if(sprite_row < 0 || sprite_row >= sprite_height_) return;

	if(fetch_sprite_buffer_->active_sprite_slot == mode_timing_.maximum_visible_sprites) {
		status_ |= StatusSpriteOverflow;
		return;
	}

	auto &sprite = fetch_sprite_buffer_->active_sprites[fetch_sprite_buffer_->active_sprite_slot];
	sprite.index = sprite_number;
	sprite.row = sprite_row >> (sprites_magnified_ ? 1 : 0);
	++fetch_sprite_buffer_->active_sprite_slot;
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

	// There are two intertwined processes here, 'fetching' (i.e. writing to the
	// line buffers with newly-fetched video contents) and 'output' (reading from
	// the line buffers and generating video).
	int fetch_cycles_pool = int_cycles;
	int output_cycles_pool = int_cycles;

	while(fetch_cycles_pool || output_cycles_pool) {
#ifndef NDEBUG
		LineBufferPointer backup = this->output_pointer_;
#endif

		if(fetch_cycles_pool) {
			// Determine how much writing to do; at the absolute most go to the end of this line.
			const int fetch_cycles = std::min(
				LineLayout<personality>::CyclesPerLine - this->fetch_pointer_.column,
				fetch_cycles_pool
			);
			const int end_column = this->fetch_pointer_.column + fetch_cycles;

			// ... and to any pending Yamaha commands.
			if constexpr (is_yamaha_vdp(personality)) {
				if(Storage<personality>::command_) {
					Storage<personality>::minimum_command_column_ =
						this->fetch_pointer_.column + Storage<personality>::command_->cycles;
					Storage<personality>::command_->cycles -= fetch_cycles;
				}
			}


			// ---------------------------------------
			// Latch scrolling position, if necessary.
			// ---------------------------------------
			if constexpr (is_sega_vdp(personality)) {
				if(!this->fetch_pointer_.row) {
					// TODO: where did this magic constant come from? https://www.smspower.org/forums/17970-RoadRashHow#111000 mentioned in passing
					// that "the vertical scroll register is latched at the start of the active display" and this is two clocks before that, so it's
					// not uncompelling. I can just no longer find my source.
					constexpr auto latch_time = LineLayout<personality>::EndOfLeftBorder - 2;
					static_assert(latch_time > 0);
					if(this->fetch_pointer_.column < latch_time && end_column >= latch_time) {
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
					this->fetch_line_buffer_->latched_horizontal_scroll = Storage<personality>::horizontal_scroll_;
				}
			}


			// ------------------------
			// Perform memory accesses.
			// ------------------------
#define fetch(function, clock, offset)	{															\
	const int first_window = from_internal<personality, clock>(this->fetch_pointer_.column);		\
	const int final_window = from_internal<personality, clock>(end_column);							\
	if(first_window == final_window) break;															\
	const auto y = uint8_t(																			\
		this->fetch_line_buffer_->vertical_state == VerticalState::Prefetch ?						\
			offset - 1 : (this->fetch_pointer_.row + offset));										\
	if(final_window != clock_rate<personality, clock>()) {											\
		function<true>(y, first_window, final_window);												\
	} else {																						\
		function<false>(y, first_window, final_window);												\
	}																								\
}



			if constexpr (is_yamaha_vdp(personality)) {
				fetch(this->template fetch_yamaha, Clock::Internal, Storage<personality>::vertical_offset_);
			} else {
				switch(this->fetch_line_buffer_->fetch_mode) {
					case FetchMode::Text:			fetch(this->template fetch_tms_text, Clock::TMSMemoryWindow, 0);		break;
					case FetchMode::Character:		fetch(this->template fetch_tms_character, Clock::TMSMemoryWindow, 0);	break;
					case FetchMode::SMS:			fetch(this->template fetch_sms, Clock::TMSMemoryWindow, 0);				break;
					case FetchMode::Refresh:		fetch(this->template fetch_tms_refresh, Clock::TMSMemoryWindow, 0);		break;

					default: break;
				}
			}

#undef fetch


			// -------------------------------
			// Check for interrupt conditions.
			// -------------------------------
			if constexpr (is_sega_vdp(personality)) {
				// The Sega VDP offers a decrementing counter for triggering line interrupts;
				// it is reloaded either when it overflows or upon every non-pixel line after the first.
				// It is otherwise decremented.
				if(
					this->fetch_pointer_.column < this->mode_timing_.line_interrupt_position &&
					end_column >= this->mode_timing_.line_interrupt_position
				) {
					if(this->fetch_pointer_.row >= 0 && this->fetch_pointer_.row <= this->mode_timing_.pixel_lines) {
						if(!this->line_interrupt_counter_) {
							this->line_interrupt_pending_ = true;
							this->line_interrupt_counter_ = this->line_interrupt_target_;
						} else {
							--this->line_interrupt_counter_;
						}
					} else {
						this->line_interrupt_counter_ = this->line_interrupt_target_;
					}
				}
			}

			if constexpr (is_yamaha_vdp(personality)) {
				// The Yamaha VDPs allow the user to specify which line an interrupt should occur on,
				// which is relative to the current vertical base. Such an interrupt will occur immediately
				// after pixels have ended.
				if(
					this->vertical_active_ &&
					this->fetch_pointer_.column < Storage<personality>::mode_description_.end_cycle &&
					end_column >= Storage<personality>::mode_description_.end_cycle &&
					this->fetch_pointer_.row == ((this->line_interrupt_target_ - Storage<personality>::vertical_offset_) & 0xff)
				) {
					this->line_interrupt_pending_ = true;
					Storage<personality>::line_matches_ = true;
				}

				if(
					this->fetch_pointer_.column < Storage<personality>::mode_description_.start_cycle &&
					end_column >= Storage<personality>::mode_description_.start_cycle
				) {
					Storage<personality>::line_matches_ = false;
				}
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
			fetch_cycles_pool -= fetch_cycles;

			// Check for end of line.
			if(this->fetch_pointer_.column == LineLayout<personality>::CyclesPerLine) {
				this->fetch_pointer_.column = 0;
				this->fetch_pointer_.row = (this->fetch_pointer_.row + 1) % this->mode_timing_.total_lines;

				this->vertical_active_ |= !this->fetch_pointer_.row;
				this->vertical_active_ &= this->fetch_pointer_.row != this->mode_timing_.pixel_lines;

				// Yamaha: handle blinking.
				if constexpr (is_yamaha_vdp(personality)) {
					if(!this->fetch_pointer_.row && Storage<personality>::blink_periods_) {
						--Storage<personality>::blink_counter_;
						while(!Storage<personality>::blink_counter_) {
							Storage<personality>::in_blink_ ^= 1;
							Storage<personality>::blink_counter_ = (Storage<personality>::blink_periods_ >> (Storage<personality>::in_blink_ << 2)) & 0xf;
						}
					}
				}

				// Progress towards any delayed events.
				this->minimum_access_column_ =
					std::max(
						0,
						this->minimum_access_column_ - LineLayout<personality>::CyclesPerLine
					);
				if constexpr (is_yamaha_vdp(personality)) {
					Storage<personality>::minimum_command_column_ =
						std::max(
							0,
							Storage<personality>::minimum_command_column_ - LineLayout<personality>::CyclesPerLine
						);
				}

				this->advance(this->fetch_line_buffer_);
				if(this->fetched_sprites_ && this->fetched_sprites_->active_sprite_slot) {
					this->fetch_line_buffer_->sprites = this->fetched_sprites_;
					this->fetched_sprites_ = nullptr;
				} else {
					this->fetch_line_buffer_->sprites = nullptr;
				}

				// Establish the current screen output mode, which will be captured as a
				// line mode momentarily.
				this->screen_mode_ = this->template current_screen_mode<true>();
				this->underlying_mode_ = this->template current_screen_mode<false>();

				if constexpr (is_yamaha_vdp(personality)) {
					auto &desc = Storage<personality>::mode_description_;
					desc.pixels_per_byte = pixels_per_byte(this->underlying_mode_);
					desc.width = width(this->underlying_mode_);
					desc.rotate_address = interleaves_banks(this->underlying_mode_);
					if(is_text(this->underlying_mode_)) {
						desc.start_cycle = LineLayout<personality>::TextModeEndOfLeftBorder;
						desc.end_cycle = LineLayout<personality>::TextModeEndOfPixels;
					} else {
						desc.start_cycle = LineLayout<personality>::EndOfLeftBorder;
						desc.end_cycle = LineLayout<personality>::EndOfPixels;
					}
				}

				// Based on the output mode, pick a line mode.
				this->fetch_line_buffer_->first_pixel_output_column = LineLayout<personality>::EndOfLeftBorder;
				this->fetch_line_buffer_->next_border_column = LineLayout<personality>::EndOfPixels;
				this->fetch_line_buffer_->pixel_count = 256;
				this->fetch_line_buffer_->screen_mode = this->screen_mode_;
				this->mode_timing_.maximum_visible_sprites = 4;
				switch(this->screen_mode_) {
					case ScreenMode::Text:
						if constexpr (is_yamaha_vdp(personality)) {
							this->fetch_line_buffer_->fetch_mode = FetchMode::Yamaha;
						} else {
							this->fetch_line_buffer_->fetch_mode = FetchMode::Text;
						}
						this->fetch_line_buffer_->first_pixel_output_column = LineLayout<personality>::TextModeEndOfLeftBorder;
						this->fetch_line_buffer_->next_border_column = LineLayout<personality>::TextModeEndOfPixels;
						this->fetch_line_buffer_->pixel_count = 240;
					break;
					case ScreenMode::YamahaText80:
						this->fetch_line_buffer_->fetch_mode = FetchMode::Yamaha;
						this->fetch_line_buffer_->first_pixel_output_column = LineLayout<personality>::TextModeEndOfLeftBorder;
						this->fetch_line_buffer_->next_border_column = LineLayout<personality>::TextModeEndOfPixels;
						this->fetch_line_buffer_->pixel_count = 480;
					break;

					case ScreenMode::SMSMode4:
						this->fetch_line_buffer_->fetch_mode = FetchMode::SMS;
						this->mode_timing_.maximum_visible_sprites = 8;
					break;

					case ScreenMode::YamahaGraphics3:
					case ScreenMode::YamahaGraphics4:
					case ScreenMode::YamahaGraphics7:
						this->fetch_line_buffer_->fetch_mode = FetchMode::Yamaha;
						this->mode_timing_.maximum_visible_sprites = 8;
					break;
					case ScreenMode::YamahaGraphics5:
					case ScreenMode::YamahaGraphics6:
						this->fetch_line_buffer_->pixel_count = 512;
						this->fetch_line_buffer_->fetch_mode = FetchMode::Yamaha;
						this->mode_timing_.maximum_visible_sprites = 8;
					break;
					default:
						// This covers both MultiColour and Graphics modes.
						if constexpr (is_yamaha_vdp(personality)) {
							this->fetch_line_buffer_->fetch_mode = FetchMode::Yamaha;
						} else {
							this->fetch_line_buffer_->fetch_mode = FetchMode::Character;
						}
					break;
				}

				if constexpr (is_yamaha_vdp(personality)) {
					this->fetch_line_buffer_->first_pixel_output_column += Storage<personality>::adjustment_[0];
					this->fetch_line_buffer_->next_border_column += Storage<personality>::adjustment_[0];
				}

				this->fetch_line_buffer_->vertical_state =
					this->screen_mode_ == ScreenMode::Blank ?
						VerticalState::Blank :
						this->vertical_state();
				const bool is_refresh = this->fetch_line_buffer_->vertical_state == VerticalState::Blank;

				Storage<personality>::begin_line(this->screen_mode_, is_refresh);

				if(is_refresh) {
					// The Yamaha handles refresh lines via its own microprogram; other VDPs
					// can fall back on the regular refresh mechanic.
					if constexpr (is_yamaha_vdp(personality)) {
						this->fetch_line_buffer_->fetch_mode = FetchMode::Yamaha;
					} else {
						this->fetch_line_buffer_->fetch_mode = FetchMode::Refresh;
					}
				}
			}
		}


#ifndef NDEBUG
		assert(backup.row == this->output_pointer_.row && backup.column == this->output_pointer_.column);
		backup = this->fetch_pointer_;
#endif


		if(output_cycles_pool) {
			// Determine how much time has passed in the remainder of this line, and proceed.
			const int target_output_cycles = std::min(
				LineLayout<personality>::CyclesPerLine - this->output_pointer_.column,
				output_cycles_pool
			);
			int output_cycles_performed = 0;
			uint32_t next_cram_value = 0;

			while(output_cycles_performed < target_output_cycles) {
				int output_cycles = target_output_cycles - output_cycles_performed;
				if(!output_cycles) continue;

				// Grab the next CRAM dot value and schedule a break in output if applicable.
				const uint32_t cram_value = next_cram_value;
				if constexpr (is_sega_vdp(personality)) {
					next_cram_value = 0;

					if(!this->upcoming_cram_dots_.empty() && this->upcoming_cram_dots_.front().location.row == this->output_pointer_.row) {
						int time_until_dot = this->upcoming_cram_dots_.front().location.column - this->output_pointer_.column;

						if(time_until_dot < output_cycles) {
							output_cycles = time_until_dot;
							next_cram_value = this->upcoming_cram_dots_.front().value;
							this->upcoming_cram_dots_.erase(this->upcoming_cram_dots_.begin());
						}
					}
				}

				output_cycles_performed += output_cycles;

				const int end_column = this->output_pointer_.column + output_cycles;


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

				const auto left_blank = [&]() {
					// Blanking region: output the entire sequence when the cursor
					// crosses the start-of-border point.
					if(
						this->output_pointer_.column < LineLayout<personality>::EndOfLeftErase &&
						end_column >= LineLayout<personality>::EndOfLeftErase
					) {
						output_sync(LineLayout<personality>::EndOfSync);
						output_blank(LineLayout<personality>::StartOfColourBurst - LineLayout<personality>::EndOfSync);
						output_default_colour_burst(LineLayout<personality>::EndOfColourBurst - LineLayout<personality>::StartOfColourBurst);
						output_blank(LineLayout<personality>::EndOfLeftErase - LineLayout<personality>::EndOfColourBurst);
					}
				};

				const auto right_blank = [&]() {
					if(end_column == LineLayout<personality>::CyclesPerLine) {
						output_blank(LineLayout<personality>::CyclesPerLine - LineLayout<personality>::EndOfRightBorder);
					}
				};

				if(this->draw_line_buffer_->vertical_state != VerticalState::Pixels) {
					if(
						this->output_pointer_.row >= this->mode_timing_.first_vsync_line &&
						this->output_pointer_.row < this->mode_timing_.first_vsync_line + 4
					) {
						// Vertical sync.
						// TODO: the Yamaha and Mega Drive both support interlaced video.
						if(end_column == LineLayout<personality>::CyclesPerLine) {
							output_sync(LineLayout<personality>::CyclesPerLine);
						}
					} else {
						left_blank();
						border(LineLayout<personality>::EndOfLeftErase, LineLayout<personality>::EndOfRightBorder);
						right_blank();
					}
				} else {
					left_blank();

					// Left border.
					border(LineLayout<personality>::EndOfLeftErase, this->draw_line_buffer_->first_pixel_output_column);

#define draw(function, clock) {																									\
	const int relative_start = from_internal<personality, clock>(start - this->draw_line_buffer_->first_pixel_output_column);	\
	const int relative_end = from_internal<personality, clock>(end - this->draw_line_buffer_->first_pixel_output_column);		\
	if(relative_start == relative_end) break;																					\
	this->function; }

					// Pixel region.
					intersect(
						this->draw_line_buffer_->first_pixel_output_column,
						this->draw_line_buffer_->next_border_column,
						if(!this->asked_for_write_area_) {
							this->asked_for_write_area_ = true;

							this->pixel_origin_ = this->pixel_target_ = reinterpret_cast<uint32_t *>(
								this->crt_.begin_data(size_t(this->draw_line_buffer_->pixel_count))
							);
						}

						if(this->pixel_target_) {
							if constexpr (is_yamaha_vdp(personality)) {
								draw(draw_yamaha(0, relative_start, relative_end), Clock::Internal);	// TODO: what is the correct 'y'?
							} else {
								switch(this->draw_line_buffer_->fetch_mode) {
									case FetchMode::SMS:			draw(draw_sms(relative_start, relative_end, cram_value), Clock::TMSPixel);			break;
									case FetchMode::Character:		draw(draw_tms_character(relative_start, relative_end), Clock::TMSPixel);			break;
									case FetchMode::Text:			draw(template draw_tms_text<false>(relative_start, relative_end), Clock::TMSPixel);	break;

									default:		break;	/* Dealt with elsewhere. */
								}
							}
						}

						if(end == this->draw_line_buffer_->next_border_column) {
							const int length = this->draw_line_buffer_->next_border_column - this->draw_line_buffer_->first_pixel_output_column;
							this->crt_.output_data(from_internal<personality, Clock::CRT>(length), size_t(this->draw_line_buffer_->pixel_count));
							this->pixel_origin_ = this->pixel_target_ = nullptr;
							this->asked_for_write_area_ = false;
						}
					);

#undef draw

					// Right border.
					border(this->draw_line_buffer_->next_border_column, LineLayout<personality>::EndOfRightBorder);

					right_blank();
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
				if(end_column == LineLayout<personality>::CyclesPerLine) {
					// Advance line buffer.
					this->advance(this->draw_line_buffer_);
				}
			}

			output_cycles_pool -= target_output_cycles;
			if(this->output_pointer_.column == LineLayout<personality>::CyclesPerLine) {
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
		border_colour = palette()[background_colour_];
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
	write_phase_ = false;

	// Enqueue the write to occur at the next available slot.
	read_ahead_buffer_ = value;
	queued_access_ = MemoryAccess::Write;
	minimum_access_column_ = fetch_pointer_.column + LineLayout<personality>::VRAMAccessDelay;
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

		case 2:	install_field<10>(pattern_name_address_, value);			break;
		case 3:	install_field<6>(colour_table_address_, value);				break;
		case 4: install_field<11>(pattern_generator_table_address_, value);	break;
		case 5:	install_field<7>(sprite_attribute_table_address_, value);	break;
		case 6:	install_field<11>(sprite_generator_table_address_, value);	break;

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
				Storage<personality>::mode_ = uint8_t(
					(Storage<personality>::mode_ & 3) |
					((value & 0xe) << 1)
				);
				enable_line_interrupts_ = value & 0x10;

				// b1–b3: M3–M5
				// b4: enable horizontal retrace interrupt
				// b5: enable light pen interrupts
				// b6: set colour bus to input or output mode
			break;

			case 1:
				Storage<personality>::mode_ = uint8_t(
					(Storage<personality>::mode_ & 0x1c) |
					((value & 0x10) >> 4) |
					((value & 0x08) >> 2)
				);
			break;

			case 7:
				Storage<personality>::background_palette_[0] = Storage<personality>::palette_[background_colour_];
			break;

			case 8:
				Storage<personality>::solid_background_ = value & 0x20;
				Storage<personality>::sprites_enabled_ = !(value & 0x02);
				if(value & 0x01) {
					LOG("TODO: Yamaha greyscale");
				}
				// b7: "1 = input on colour bus, enable mouse; 1 = output on colour bus, disable mouse" [documentation clearly in error]
				// b6: 1 = enable light pen
				// b5: sets the colour of code 0 to the colour of the palette (???)
				// b4: 1 = colour bus in input mode; 0 = colour bus in output mode
				// b3: 1 = VRAM is 64kx1 or 64kx4; 0 = 16kx1 or 16kx4; affects refresh.
				// b1: 1 = disable sprites (and release sprite access slots)
				// b0: 1 = output in grayscale
			break;

			case 9:
				mode_timing_.pixel_lines = (value & 0x80) ? 212 : 192;
				mode_timing_.end_of_frame_interrupt_position.row = mode_timing_.pixel_lines+1;
				// TODO: on the Yamaha, at least, tie this interrupt overtly to vertical state.

				if(value & 0x08) {
					LOG("TODO: Yamaha interlace mode");
				}

				// b7: 1 = 212 lines of pixels; 0 = 192
				// b5 & b4: select simultaneous mode (seems to relate to line length and in-phase colour?)
				// b3: 1 = interlace on
				// b2: 1 = display two graphic screens interchangeably by even/odd field
				// b1: 1 = PAL mode; 0 = NTSC mode
				// b0: 1 = [dot clock] DLCLK is input; 0 = DLCLK is output
			break;

			// b0–b2: A14–A16 of the colour table.
			case 10:	install_field<14>(colour_table_address_, value);			break;

			// b0–b1: A15–A16 of the sprite table.
			case 11:	install_field<15>(sprite_attribute_table_address_, value);	break;

			case 12:
				Storage<personality>::blink_text_colour_ = value >> 4;
				Storage<personality>::blink_background_colour_ = value & 0xf;
				// as per register 7, but in blink mode.
			break;

			case 13:
				Storage<personality>::blink_periods_ = value;
				if(!value) {
					Storage<personality>::in_blink_ = 0;
				}

				// b0–b3: display time for odd page;
				// b4–b7: display time for even page.
			break;

			case 14:	install_field<14>(ram_pointer_, value);						break;

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
				Storage<personality>::adjustment_[0] = (8 - ((value & 15) ^ 8)) * 4;
				Storage<personality>::adjustment_[1] = 8 - ((value >> 4) ^ 8);
				// b0-b3: horizontal adjustment
				// b4-b7: vertical adjustment
			break;

			case 19:
				line_interrupt_target_ = value;
				// b0–b7: line to match for interrupts (if eabled)
			break;

			case 20:
			case 21:
			case 22:
//				LOG("TODO: Yamaha colour burst selection; " << PADHEX(2) << +value);
				// Documentation is "fill with 0s for no colour burst; magic pattern for colour burst"
			break;

			case 23:
				Storage<personality>::vertical_offset_ = value;
			break;

			case 32:	Storage<personality>::command_context_.source.template set<0, false>(value);		break;
			case 33:	Storage<personality>::command_context_.source.template set<0, true>(value);			break;
			case 34:	Storage<personality>::command_context_.source.template set<1, false>(value);		break;
			case 35:	Storage<personality>::command_context_.source.template set<1, true>(value);			break;

			case 36:	Storage<personality>::command_context_.destination.template set<0, false>(value);	break;
			case 37:	Storage<personality>::command_context_.destination.template set<0, true>(value);	break;
			case 38:	Storage<personality>::command_context_.destination.template set<1, false>(value);	break;
			case 39:	Storage<personality>::command_context_.destination.template set<1, true>(value);	break;

			case 40:	Storage<personality>::command_context_.size.template set<0, false>(value);			break;
			case 41:	Storage<personality>::command_context_.size.template set<0, true>(value);			break;
			case 42:	Storage<personality>::command_context_.size.template set<1, false>(value);			break;
			case 43:	Storage<personality>::command_context_.size.template set<1, true>(value);			break;

			case 44:
				Storage<personality>::command_context_.colour.set(value);

				// Check whether a command was blocked on this.
				if(
					Storage<personality>::command_ &&
					Storage<personality>::command_->access == Command::AccessType::WaitForColourReceipt
				) {
					Storage<personality>::command_->advance();
					Storage<personality>::update_command_step(fetch_pointer_.column);
				}
			break;

			case 45:
				Storage<personality>::command_context_.arguments = value;
				// b6: MXC, i.e. destination for INed/OUTed video data; 0 = video RAM; 1 = expansion RAM.
				// b5: MXD, destination for command engine.
				// b4: MXS, source for command engine.
				// b3: DIY
				// b2: DIX
				// b1: EQ
				// b0: MAJ
			break;

			case 46:
				// b0–b3: LO0–LO3 (i.e. operation to apply if this is a logical command)
				// b4–b7: CM0-CM3 (i.e. command to perform)

				// If a command is already ongoing and this is not a stop, ignore it.
				if(Storage<personality>::command_ && (value >> 4) != 0b0000) {
					break;
				}

#define Begin(x)	Storage<personality>::command_ = std::make_unique<Commands::x>(Storage<personality>::command_context_, Storage<personality>::mode_description_);
				using MoveType = Commands::MoveType;
				switch(value >> 4) {
					// All codes not listed below are invalid; treat them as STOP.
					default:
					case 0b0000:	Storage<personality>::command_ = nullptr;	break;	// STOP.

					case 0b0100:	Begin(Point<true>);							break;	// POINT [read a pixel colour].
					case 0b0101:	Begin(Point<false>);						break;	// PSET [plot a pixel].
					case 0b0110:	break;	// TODO: srch.	[search horizontally for a colour]
					case 0b0111:	Begin(Line);								break;	// LINE [draw a Bresenham line].

					case 0b1000:	Begin(Fill<true>);							break;	// LMMV [logical move, VDP to VRAM, i.e. solid-colour fill].
					case 0b1001:	Begin(Move<MoveType::Logical>);				break;	// LMMM [logical move, VRAM to VRAM].
					case 0b1010:	break;	// TODO: lmcm.	[logical move, VRAM to CPU]
					case 0b1011:	Begin(MoveFromCPU<true>);					break;	// LMMC [logical move, CPU to VRAM].

					case 0b1100:	Begin(Fill<false>);							break;	// HMMV [high-speed move, VDP to VRAM, i.e. single-byte fill].
					case 0b1101:	Begin(Move<MoveType::HighSpeed>);			break;	// HMMM [high-speed move, VRAM to VRAM].
					case 0b1110:	Begin(Move<MoveType::YOnly>);				break;	// YMMM [high-speed move, y only, VRAM to VRAM].
					case 0b1111:	Begin(MoveFromCPU<false>);					break;	// HMMC [high-speed move, CPU to VRAM].
				}
#undef Begin

				Storage<personality>::command_context_.pixel_operation = CommandContext::LogicalOperation(value & 7);
				Storage<personality>::command_context_.test_source = value & 8;

				// Kill the command immediately if it's done in zero operations
				// (e.g. a line of length 0).
				if(!Storage<personality>::command_ && (value >> 4)) {
					LOG("TODO: Yamaha command " << PADHEX(2) << +value);
				}

				// Seed timing information if a command was found.
				Storage<personality>::update_command_step(fetch_pointer_.column);
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

		// The initial write should half update the access pointer, other than
		// on the Yamaha.
		if constexpr (!is_yamaha_vdp(personality)) {
			install_field<0>(ram_pointer_, value);
		}
		return;
	}

	// The RAM pointer is always set on a second write, regardless of
	// whether the caller is intending to enqueue a VDP operation.
	// If this isn't a Yamaha VDP then the RAM address is updated
	// regardless of whether this turns out to be a register access.
	//
	// The top two bits are used to determine the type of write; only
	// the lower six are actual address.
	if constexpr (!is_yamaha_vdp(personality)) {
		install_field<8, 6>(ram_pointer_, value);
	}

	write_phase_ = false;
	if(value & 0x80) {
		commit_register(value, low_write_);
	} else {
		// This is an access via the RAM pointer; if this is a Yamaha VDP then update
		// the low 14-bits of the RAM pointer now.
		if constexpr (is_yamaha_vdp(personality)) {
			install_field<0>(ram_pointer_, low_write_);
			install_field<8, 6>(ram_pointer_, value);
		}

		if(!(value & 0x40)) {
			// A read request is enqueued upon setting the address; conversely a write
			// won't be enqueued unless and until some actual data is supplied.
			queued_access_ = MemoryAccess::Read;
			minimum_access_column_ = fetch_pointer_.column + LineLayout<personality>::VRAMAccessDelay;
		}

		if constexpr (is_sega_vdp(personality)) {
			Storage<personality>::cram_is_selected_ = false;
		}
	}
}

template <Personality personality>
void Base<personality>::write_palette(uint8_t value) {
	if constexpr (is_yamaha_vdp(personality)) {
		if(!Storage<personality>::palette_write_phase_) {
			Storage<personality>::new_colour_ = value;
			Storage<personality>::palette_write_phase_ = true;
			return;
		}

		Storage<personality>::palette_write_phase_ = false;

		const uint8_t r = ((Storage<personality>::new_colour_ >> 4) & 7) * 255 / 7;
		const uint8_t g = (value & 7) * 255 / 7;
		const uint8_t b = (Storage<personality>::new_colour_ & 7) * 255 / 7;

		Storage<personality>::palette_[Storage<personality>::palette_entry_ & 0xf] = palette_pack(r, g, b);
		Storage<personality>::background_palette_[Storage<personality>::palette_entry_ & 0xf] = palette_pack(r, g, b);
		Storage<personality>::background_palette_[0] = Storage<personality>::palette_[background_colour_];

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
			default:
			case 0: break;

			case 1: {
				// b7 = light pen; set when light is detected, reset on read;
				//		or: mouse button 2 currently down.
				// b6 = light pen button or mouse button 1.
				// b5–b1 = VDP identification (0 = 9938; 2 = 9958)
				// b0 = set when the VDP reaches the line provided in the line interrupt register.
				//		Reset upon read.
				const uint8_t result =
					(personality == Personality::V9938 ? 0x0 : 0x4) |
					((line_interrupt_pending_ && enable_line_interrupts_) ? 0x01 : 0x00);

				line_interrupt_pending_ = false;
				return result;
			} break;

			case 2: {
				// b7 = transfer ready flag (i.e. VDP ready for next transfer)
				// b6 = 1 during vblank
				// b5 = 1 during hblank
				// b4 = set if colour detected during search command
				// b1 = display field odd/even
				// b0 = command ongoing
				const uint8_t transfer_ready =
					(queued_access_ == MemoryAccess::None ? 0x80 : 0x00) &
					((
						!Storage<personality>::command_ ||
						!Storage<personality>::command_->is_cpu_transfer ||
						Storage<personality>::command_->access == Command::AccessType::WaitForColourReceipt
					) ? 0x80 : 0x00);

				return
					transfer_ready |
					(vertical_state() != VerticalState::Pixels ? 0x40 : 0x00) |
					(is_horizontal_blank() ? 0x20 : 0x00) |
					(Storage<personality>::command_ ? 0x01 : 0x00);

			} break;

			case 3:	return uint8_t(Storage<personality>::collision_location_[0]);
			case 4:	return uint8_t((Storage<personality>::collision_location_[0] >> 8) | 0xfe);
			case 5:	return uint8_t(Storage<personality>::collision_location_[1]);
			case 6:	return uint8_t((Storage<personality>::collision_location_[1] >> 8) | 0xfc);

			case 7:	return Storage<personality>::colour_status_;

			case 8:	return uint8_t(Storage<personality>::colour_location_);
			case 9:	return uint8_t((Storage<personality>::colour_location_ >> 8) | 0xfe);
		}
	}

	// Gets the status register.
	const uint8_t result = status_;
	status_ &= ~(StatusInterrupt | StatusSpriteOverflow | StatusSpriteCollision);
	if constexpr (is_sega_vdp(personality)) {
		line_interrupt_pending_ = false;
	}
	return result;
}

template <Personality personality>
uint8_t TMS9918<personality>::read(int address) {
	const int target = this->masked_address(address);

	if(target < 2) {
		this->write_phase_ = false;
	}

	switch(target) {
		default: return 0xff;
		case 0:	return this->read_vram();
		case 1:	return this->read_register();
	}
}

// MARK: - Ephemeral state.

template <Personality personality>
int Base<personality>::fetch_line() const {
	// This is the proper Master System value; TODO: what's correct for Yamaha, etc?
	constexpr int row_change_position = 31;

	return
		(this->fetch_pointer_.column < row_change_position)
			? (this->fetch_pointer_.row + this->mode_timing_.total_lines - 1) % this->mode_timing_.total_lines
			: this->fetch_pointer_.row;
}

template <Personality personality>
VerticalState Base<personality>::vertical_state() const {
	if(vertical_active_) {
		return VerticalState::Pixels;
	} else if(fetch_pointer_.row == mode_timing_.total_lines - 1) {
		return VerticalState::Prefetch;
	} else {
		return VerticalState::Blank;
	}
}

template <Personality personality>
bool Base<personality>::is_horizontal_blank() const {
	return fetch_pointer_.column < LineLayout<personality>::EndOfLeftErase || fetch_pointer_.column >= LineLayout<personality>::EndOfRightBorder;
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
	const int frame_length = LineLayout<personality>::CyclesPerLine * this->mode_timing_.total_lines;
	int time_until_frame_interrupt =
		(
			((this->mode_timing_.end_of_frame_interrupt_position.row * LineLayout<personality>::CyclesPerLine) + this->mode_timing_.end_of_frame_interrupt_position.column + frame_length) -
			((this->fetch_pointer_.row * LineLayout<personality>::CyclesPerLine) + this->fetch_pointer_.column)
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
		cycles_to_next_interrupt_threshold += LineLayout<personality>::CyclesPerLine;
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

	if constexpr (is_yamaha_vdp(personality)) {
		next_line_interrupt_row = (this->line_interrupt_target_ - Storage<personality>::vertical_offset_) & 0xff;
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
	const int lines_until_interrupt = (next_line_interrupt_row - line_of_next_interrupt_threshold + this->mode_timing_.total_lines) % this->mode_timing_.total_lines;
	const int local_cycles_until_line_interrupt = cycles_to_next_interrupt_threshold + lines_until_interrupt * LineLayout<personality>::CyclesPerLine;
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
		cycles_to_next_interrupt_threshold += LineLayout<personality>::CyclesPerLine;
		++line_of_next_interrupt_threshold;
	}

	if(line_of_next_interrupt_threshold > line) {
		line += this->mode_timing_.total_lines;
	}

	return this->clock_converter_.half_cycles_before_internal_cycles(cycles_to_next_interrupt_threshold + (line - line_of_next_interrupt_threshold)*LineLayout<personality>::CyclesPerLine);
}

template <Personality personality>
bool TMS9918<personality>::get_interrupt_line() const {
	return
		((this->status_ & StatusInterrupt) && this->generate_interrupts_) ||
		(this->enable_line_interrupts_ && this->line_interrupt_pending_);
}

// TODO: [potentially] remove Master System timing assumptions in latch and get_latched below, if any other VDP uses these calls.
template <Personality personality>uint8_t TMS9918<personality>::get_latched_horizontal_counter() const {
	// Translate from internal numbering to the public numbering,
	// which counts the 256 pixels as items 0–255, starts
	// counting at -48, and returns only the top 8 bits of the number.
	int public_counter = this->latched_column_ - LineLayout<personality>::EndOfLeftBorder;
	if(public_counter < -46) public_counter += LineLayout<personality>::CyclesPerLine;
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
