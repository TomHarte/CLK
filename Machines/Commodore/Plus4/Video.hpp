//
//  Video.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/12/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#pragma once

#include "Interrupts.hpp"
#include "Pager.hpp"

#include "Numeric/UpperBound.hpp"
#include "Outputs/CRT/CRT.hpp"

#include <algorithm>
#include <array>
#include <bit>

namespace Commodore::Plus4 {

constexpr int clock_rate(bool is_ntsc) {
	return is_ntsc ?
				14'318'180 :	// i.e. colour subcarrier * 4.
				17'734'448;		// i.e. very close to colour subcarrier * 4 — only about 0.1% off.
}

struct Video {
public:
	Video(const Commodore::Plus4::Pager &pager, Interrupts &interrupts) :
		crt_(465, 1, Outputs::Display::Type::PAL50, Outputs::Display::InputDataType::Luminance8Phase8),
		pager_(pager),
		interrupts_(interrupts)
	{
		const auto visible_lines = 33 * 8;
		const auto centre = eos() - vs_stop() + 104;	// i.e. centre on vertical_counter_ = 104.

		crt_.set_visible_area(crt_.get_rect_for_area(
			centre - (visible_lines / 2),
			visible_lines,
			int(HorizontalEvent::Begin40Columns) - int(HorizontalEvent::BeginSync) + int(HorizontalEvent::ScheduleCounterReset) + 1 - 8,
			int(HorizontalEvent::End40Columns) - int(HorizontalEvent::Begin40Columns) + 16,
			4.0f / 3.0f
		));
	}

	template <uint16_t address>
	uint8_t read() const {
		switch(address) {
			case 0xff06:	return ff06_;
			case 0xff07:	return ff07_;
			case 0xff0a:	return (raster_interrupt_ >> 8) & 1;
			case 0xff0b:	return uint8_t(raster_interrupt_);
			case 0xff0c:	return (cursor_position_ >> 8) | 0xfc;
			case 0xff0d:	return uint8_t(cursor_position_);
			case 0xff14:	return uint8_t((video_matrix_base_ >> 8) & 0xf8) | 0x07;

			case 0xff15:	case 0xff16:	case 0xff17:	case 0xff18:	case 0xff19:
				return 0x80 | raw_background_[size_t(address - 0xff15)];

			case 0xff1a:	return uint8_t(character_position_reload_ >> 8) | 0xfc;
			case 0xff1b:	return uint8_t(character_position_reload_);
			case 0xff1c:	return uint8_t(vertical_counter_ >> 8) | 0xfe;
			case 0xff1d:	return uint8_t(vertical_counter_);
			case 0xff1e:	return uint8_t(horizontal_counter_ >> 1);
			case 0xff1f:
				return uint8_t(
					((flash_count_ & 0xf) << 3) |
					vertical_sub_count_
				) | 0x80;
		}

		return 0xff;
	}

	template <uint16_t address>
	void write(const uint8_t value) {
		const auto load_high10 = [&](uint16_t &target) {
			target = uint16_t(
				(target & 0x00ff) | ((value & 0x3) << 8)
			);
		};
		const auto load_low8 = [&](uint16_t &target) {
			target = uint16_t(
				(target & 0xff00) | value
			);
		};
		const auto set_video_mode = [&] {
			if(bitmap_mode_) {
				if(extended_colour_mode_) {
					video_mode_ = VideoMode::Blank;
				} else if(multicolour_mode_) {
					video_mode_ = VideoMode::MulticolourBitmap;
				} else {
					video_mode_ = VideoMode::HighResBitmap;
				}
			} else {
				if(multicolour_mode_) {
					video_mode_ = extended_colour_mode_ ? VideoMode::Blank : VideoMode::MulticolourText;
				} else if(extended_colour_mode_) {
					video_mode_ = VideoMode::ExtendedColourText;
				} else {
					video_mode_ = VideoMode::Text;
				}
			}
		};

		switch(address) {
			case 0xff06:
				ff06_ = value;
				extended_colour_mode_ = value & 0x40;
				bitmap_mode_ = value & 0x20;
				display_enable_ = value & 0x10;
				rows_25_ = value & 8;
				y_scroll_ = value & 7;
				set_video_mode();
			break;

			case 0xff07:
				ff07_ = value;
				characters_256_ = value & 0x80;
				is_ntsc_ = value & 0x40;
				ted_off_ = value & 0x20;
				multicolour_mode_ = value & 0x10;
				columns_40_ = value & 8;
				x_scroll_ = value & 7;
				set_video_mode();

				if(characters_256_) {
					character_base_mask_ = 0xf800;
					character_mask_ = 0xff;
					inversion_mask_ = 0x00;
				} else {
					character_base_mask_ = 0xfc00;
					character_mask_ = 0x7f;
					inversion_mask_ = 0xff;
				}
			break;

			case 0xff0a:
				raster_interrupt_ = (raster_interrupt_ & 0x00ff) | ((value & 1) << 8);
			break;
			case 0xff0b:
				raster_interrupt_ = (raster_interrupt_ & 0xff00) | value;
			break;

			case 0xff0c:	load_high10(cursor_position_);				break;
			case 0xff0d:	load_low8(cursor_position_);				break;

			case 0xff12:
				bitmap_base_ = uint16_t((value & 0x38) << 10);
			break;
			case 0xff13:
				character_base_ = uint16_t((value & 0xfc) << 8);
				single_clock_ = value & 0x02;
			break;
			case 0xff14:
				video_matrix_base_ = uint16_t((value & 0xf8) << 8);
			break;

			case 0xff15:	case 0xff16:	case 0xff17:	case 0xff18:	case 0xff19:
				raw_background_[size_t(address - 0xff15)] = value;
				background_[size_t(address - 0xff15)] = colour(value);
			break;

			case 0xff1a:	load_high10(character_position_reload_);	break;
			case 0xff1b:	load_low8(character_position_reload_);		break;

			case 0xff1c:	vertical_counter_ = (vertical_counter_ & 0x00ff) | ((value & 1) << 8);		break;
			case 0xff1d:	vertical_counter_ = (vertical_counter_ & 0xff00) | value;					break;
			case 0xff1e:
				// TODO: possibly should be deferred, if falling out of phase?
				horizontal_counter_ = (horizontal_counter_ & 0x07) | ((~value << 1) & ~0x07);
				horizontal_counter_ &= 0x1ff;
			break;
			case 0xff1f:
				vertical_sub_count_ = value & 0x7;
				flash_count_ = (flash_count_ & 0x10) | ((value >> 3) & 0xf);
			break;
		}
	}

	Cycles cycle_length([[maybe_unused]] bool is_ready) const {
		if(is_ready) {
//			return
//				Cycles(EndCharacterFetchWindow - horizontal_counter_ + EndOfLine) * is_ntsc_ ? Cycles(4) : Cycles(5) / 2;
		}

		const bool is_long_cycle = single_clock_ || refresh_ || (external_fetch_ && enable_display_);

		if(is_ntsc_) {
			return is_long_cycle ? Cycles(16) : Cycles(8);
		} else {
			return is_long_cycle ? Cycles(20) : Cycles(10);
		}
	}

	Cycles timer_cycle_length() const {
		return is_ntsc_ ? Cycles(16) : Cycles(20);
	}

	// Outer clock is [NTSC or PAL] colour subcarrier * 2.
	//
	// 65 cycles = 64µs?
	// 65*262*60 = 1021800
	//
	// In an NTSC television system. 262 raster lines are produced (0 to 261), 312 for PAL (0−311).
	//
	// An interrupt is generated 8 cycles before the character window. For a 25 row display, the visible
	// raster lines are from 4 to 203.
	//
	// The horizontal position register counts 456 dots, 0 to 455.
	void run_for(Cycles cycles) {
		// Timing:
		//
		// Input clock is at 17.7Mhz PAL or 14.38Mhz NTSC. i.e. each is four times the colour subcarrier.
		//
		// In PAL mode, divide by 5 and multiply by 2 to get the internal pixel clock.
		//
		// In NTSC mode just dividing by 2 would do to get the pixel clock but in practice that's implemented as
		// a divide by 4 and a multiply by 2 to keep it similar to the PAL code.
		//
		// That gives close enough to 456 pixel clocks per line in both systems so the TED just rolls with that.

		subcycles_ += cycles * 2;
		auto ticks_remaining = subcycles_.divide(is_ntsc_ ? Cycles(4) : Cycles(5)).as<int>();
		while(ticks_remaining) {
			//
			// Check for events: (i) deferred; ...
			//
			if(delayed_events_) {
				if(delayed_events_ & uint64_t(DelayedEvent::Latch)) {
					if(char_pos_latch_ && vertical_sub_active_) {
						character_position_reload_ = character_position_;
					}
					char_pos_latch_ = vertical_sub_count_ == 6;
					if(char_pos_latch_ && enable_display_) {
						video_counter_reload_ = video_counter_;
					}
				}

				if(delayed_events_ & uint64_t(DelayedEvent::Flash)) {
					if(vertical_counter_ == 205) {
						++flash_count_;
						flash_mask_ = (flash_count_ & 0x10) ? 0xff : 0x00;
					}
				}

				if(delayed_events_ & uint64_t(DelayedEvent::IncrementVerticalLine)) {
					vertical_counter_ = next_vertical_counter_;
					bad_line2_ = bad_line();
				}

				if(delayed_events_ & uint64_t(DelayedEvent::IncrementVerticalSub)) {
					if(!video_line_) {
						vertical_sub_count_ = 7;	// TODO: should be between cycle 0xc8 and 0xca?
					} else if(display_enable_ && vertical_sub_active_) {
						vertical_sub_count_ = (vertical_sub_count_ + 1) & 7;
					}
				}

				if(delayed_events_ & uint64_t(DelayedEvent::CounterReset)) {
					horizontal_counter_ = 0;
				}

				delayed_events_ &= ~uint64_t(DelayedEvent::Mask);
			}

			// ... (ii) timer-linked.
			switch(HorizontalEvent(horizontal_counter_)) {
				case HorizontalEvent::CounterOverflow:
					horizontal_counter_ = 0;
					[[fallthrough]];
				case HorizontalEvent::Begin40Columns:
					if(vertical_screen_ && enable_display_) wide_screen_ = true;
				break;
				case HorizontalEvent::End40Columns:
					if(vertical_screen_ && enable_display_) wide_screen_ = false;
				break;
				case HorizontalEvent::Begin38Columns:
					if(vertical_screen_ && enable_display_) narrow_screen_ = true;
				break;
				case HorizontalEvent::End38Columns:
					if(vertical_screen_ && enable_display_) narrow_screen_ = false;
					video_shift_ = false;
				break;
				case HorizontalEvent::DMAWindowEnd:				dma_window_ = false;		break;
				case HorizontalEvent::EndRefresh:				refresh_ = false;			break;
				case HorizontalEvent::EndCharacterFetchWindow:	character_window_ = false;	break;
				case HorizontalEvent::BeginBlank:				horizontal_blank_ = true;	break;
				case HorizontalEvent::BeginSync:				horizontal_sync_ = true;	break;
				case HorizontalEvent::EndSync:					horizontal_sync_ = false;	break;

				case HorizontalEvent::LatchCharacterPosition:	schedule<8>(DelayedEvent::Latch);			break;
				case HorizontalEvent::IncrementFlashCounter:	schedule<4>(DelayedEvent::Flash);			break;
				case HorizontalEvent::EndOfScreen:
					schedule<8>(DelayedEvent::IncrementVerticalLine);
					next_vertical_counter_ = video_line_ == eos() ? 0 : ((vertical_counter_ + 1) & 511);
					horizontal_burst_ = true;
				break;

				case HorizontalEvent::EndExternalFetchWindow:
					external_fetch_ = false;
					increment_character_position_ = false;
					if(enable_display_) increment_video_counter_ = false;
					refresh_ = true;
				break;

				case HorizontalEvent::VerticalSubActive:
					if(bad_line()) {
						vertical_sub_active_ = true;
					} else if(!enable_display_) {
						vertical_sub_active_ = false;
					}
				break;

				case HorizontalEvent::IncrementVerticalSub:
					schedule<8>(DelayedEvent::IncrementVerticalSub);
					video_line_ = vertical_counter_;
					character_position_ = 0;

					if(video_line_ == eos()) {
						character_position_reload_ = 0;
						video_counter_reload_ = 0;
					}
				break;

				case HorizontalEvent::ScheduleCounterReset:
					schedule<1>(DelayedEvent::CounterReset);
				break;

				case HorizontalEvent::BeginExternalFetchClock:
					external_fetch_ = true;

					if(video_line_ == vs_start()) {
						vertical_sync_ = true;
					} else if(video_line_ == vs_stop()) {
						vertical_sync_ = false;
					}
				break;

				case HorizontalEvent::BeginAttributeFetch:
					dma_window_ = true;
					horizontal_burst_ = false;	// Should be 1 cycle later, if the data sheet is completely accurate.
												// Though all other timings work on the assumption that it isn't.
				break;

				case HorizontalEvent::EndBlank:
					horizontal_blank_ = false;
				break;

				case HorizontalEvent::IncrementVideoCounter:
					increment_character_position_ = true;
					if(enable_display_) increment_video_counter_ = true;

					if(enable_display_ && vertical_sub_active_) {
						character_position_ = character_position_reload_;
					}
					video_counter_ = video_counter_reload_;
				break;

				case HorizontalEvent::BeginShiftRegister:
					if(enable_display_) {
						character_window_ = video_shift_ = true;
					}
					output_.reset();
				break;
			}

			// Test for raster interrupt.
			if(raster_interrupt_ == vertical_counter_) {
				if(!raster_interrupt_done_) {
					raster_interrupt_done_ = true;
					interrupts_.apply(Interrupts::Flag::Raster);
				}
			} else {
				raster_interrupt_done_ = false;
			}

			//
			// Compute time to run for in this step based upon:
			//	(i) timer-linked events;
			//	(ii) deferred events; and
			//	(iii) ticks remaining.
			//
			const auto next = Numeric::upper_bound<
				int(HorizontalEvent::Begin40Columns),			int(HorizontalEvent::Begin38Columns),
				int(HorizontalEvent::LatchCharacterPosition),	int(HorizontalEvent::DMAWindowEnd),
				int(HorizontalEvent::EndExternalFetchWindow),	int(HorizontalEvent::EndCharacterFetchWindow),
				int(HorizontalEvent::End38Columns),				int(HorizontalEvent::End40Columns),
				int(HorizontalEvent::EndRefresh),				int(HorizontalEvent::IncrementFlashCounter),
				int(HorizontalEvent::BeginBlank),				int(HorizontalEvent::BeginSync),
				int(HorizontalEvent::VerticalSubActive),		int(HorizontalEvent::EndOfScreen),
				int(HorizontalEvent::EndSync),					int(HorizontalEvent::IncrementVerticalSub),
				int(HorizontalEvent::BeginExternalFetchClock),	int(HorizontalEvent::BeginAttributeFetch),
				int(HorizontalEvent::EndBlank),					int(HorizontalEvent::IncrementVideoCounter),
				int(HorizontalEvent::BeginShiftRegister),		int(HorizontalEvent::ScheduleCounterReset),
				int(HorizontalEvent::CounterOverflow)
			>(horizontal_counter_);
			const auto period = [&] {
				auto period = std::min(next - horizontal_counter_, ticks_remaining);
				if(delayed_events_) {
					period = std::min(period, std::countr_zero(delayed_events_) / DelayEventSize);
				}
				return period;
			}();

			// Update vertical state.
			if(rows_25_) {
				if(video_line_ == 4) vertical_screen_ = true;
				else if(video_line_ == 204) vertical_screen_ = false;
			} else {
				if(video_line_ == 8) vertical_screen_ = true;
				else if(video_line_ == 200) vertical_screen_ = false;
			}

			character_fetch_ |= bad_line2_;
			if(video_line_ == vblank_start()) vertical_blank_ = true;
			else if(video_line_ == vblank_stop()) vertical_blank_ = false;
			else if(video_line_ == 0 && display_enable_) enable_display_ = true;
			else if(video_line_ == 204) {
				enable_display_ = false;
				character_fetch_ = false;
			}

			//
			// Output.
			//
			OutputState state;
			if(vertical_sync_ || horizontal_sync_) {
				state = OutputState::Sync;
			} else if(vertical_blank_ || horizontal_blank_) {
				state = horizontal_burst_ ? OutputState::Burst : OutputState::Blank;
			} else {
				const bool pixel_screen = columns_40_ ? wide_screen_ : narrow_screen_;
				state = enable_display_ && pixel_screen ? OutputState::Pixels : OutputState::Border;
			}

			static constexpr auto PixelAllocationSize = 320;
			if(state != output_state_ || (state == OutputState::Pixels && time_in_state_ == PixelAllocationSize)) {
				switch(output_state_) {
					case OutputState::Blank:	crt_.output_blank(time_in_state_);								break;
					case OutputState::Sync:		crt_.output_sync(time_in_state_);								break;
					case OutputState::Burst:	crt_.output_default_colour_burst(time_in_state_);				break;
					case OutputState::Border:	crt_.output_level<uint16_t>(time_in_state_, background_[4]);	break;
					case OutputState::Pixels:	crt_.output_data(time_in_state_, size_t(time_in_state_));		break;
				}
				time_in_state_ = 0;

				output_state_ = state;
				if(output_state_ == OutputState::Pixels) {
					pixels_ = reinterpret_cast<uint16_t *>(crt_.begin_data(PixelAllocationSize));
				} else {
					pixels_ = nullptr;
				}
			}

			// Get count of 'single_cycle_end's in FPGATED parlance.
			const int start_window = horizontal_counter_ >> 3;
			const int end_window = (horizontal_counter_ + period) >> 3;
			const int window_count = end_window - start_window;

			// Advance DMA state machine.
			for(int cycle = 0; cycle < window_count; cycle++) {
				const auto is_active = [&] {	return dma_window_ && (bad_line2_ || bad_line());	};
				const auto set_idle = [&] {
					dma_state_ = DMAState::IDLE;
					interrupts_.bus().set_ready_line(false);
				};
				switch(dma_state_) {
					case DMAState::IDLE:
						if(is_active()) {
							dma_state_ = DMAState::THALT1;
						}
					break;
					case DMAState::THALT1:
					case DMAState::THALT2:
					case DMAState::THALT3:
						if(is_active()) {
							dma_state_ = DMAState(int(dma_state_) + 1);
							interrupts_.bus().set_ready_line(true);
						} else {
							set_idle();
						}
					break;
					case DMAState::TDMA:
						if(!is_active()) {
							set_idle();
						}
					break;
				}
				if(video_shift_ || wide_screen_) {
					next_attribute_.advance();
					next_character_.advance();
					next_pixels_.advance();

					const bool is_2bpp =
						(video_mode_ == VideoMode::MulticolourBitmap) ||
						(video_mode_ == VideoMode::MulticolourText && output_.attributes<0>() & 0x8);
					const int adjustment = (x_scroll_ & 1) && is_2bpp;
					output_.load_pixels(next_pixels_.read(), x_scroll_ + adjustment);
				}
				if(increment_video_counter_) {
					//
					// If this is one of the relevant bad lines then obtain a new character index and attributes,
					// placing them into the delaying shift registers.
					//
					const uint8_t character = shifter_.read<0>();
					next_character_.write(character);

					const auto address = [&] { return uint16_t(video_matrix_base_ + video_counter_); };
					if(bad_line()) {
						shifter_.write<0>(pager_.read(address() + 0x400));
					} else if(bad_line2_) {
						shifter_.write<1>(pager_.read(address()));
					}

					next_attribute_.write(shifter_.read<1>());

					const auto cursor = [&]() -> uint8_t {
						return
							(
								(!cursor_position_ && !character_position_) ||
								((character_position_ == cursor_position_) && vertical_sub_active_)
							)
								? flash_mask_ : 0x00;
					};

					//
					// Also obtain pixel data, which is a function of current character in text modes but not
					// in bitmap modes.
					//
					uint8_t pixels = 0;
					switch(video_mode_) {
						case VideoMode::Blank:
						break;

						case VideoMode::Text:
						case VideoMode::MulticolourText:
							pixels = pager_.read(uint16_t(
								(character_base_ & character_base_mask_) +
								((character & character_mask_) << 3)
								+ vertical_sub_count_
							)) ^ cursor();
						break;

						case VideoMode::ExtendedColourText:
							pixels = pager_.read(uint16_t(
								character_base_ + ((character & 0x3f) << 3) + vertical_sub_count_
							)) ^ cursor();
						break;

						case VideoMode::MulticolourBitmap:
						case VideoMode::HighResBitmap:
							pixels = pager_.read(uint16_t(
								bitmap_base_ + (character_position_ << 3) + vertical_sub_count_
							));
						break;
					}

					next_pixels_.write(pixels);
					shifter_.advance();
					video_counter_ = (video_counter_ + 1) & 0x3ff;
				}
				if(increment_character_position_ && character_fetch_) {
					character_position_ = (character_position_ + 1) & 0x3ff;
				}
				if(enable_display_) {
					switch(x_scroll_) {
						case 0:	draw<0>();	break;
						case 1:	draw<1>();	break;
						case 2:	draw<2>();	break;
						case 3:	draw<3>();	break;
						case 4:	draw<4>();	break;
						case 5:	draw<5>();	break;
						case 6:	draw<6>();	break;
						case 7:	draw<7>();	break;
					}
				}
			}

			// Advance for the current period.
			time_in_state_ += period;
			horizontal_counter_ += period;
			delayed_events_ >>= period * DelayEventSize;
			ticks_remaining -= period;
		}
	}

	void set_scan_target(Outputs::Display::ScanTarget *const target) {
		crt_.set_scan_target(target);
	}

	Outputs::Display::ScanStatus get_scaled_scan_status() const {
		return crt_.get_scaled_scan_status();
	}

	void set_display_type(const Outputs::Display::DisplayType display_type) {
		crt_.set_display_type(display_type);
	}

	Outputs::Display::DisplayType get_display_type() const {
		return crt_.get_display_type();
	}

private:
	Outputs::CRT::CRT crt_;
	Cycles subcycles_;

	// Programmable values.
	bool extended_colour_mode_ = false;
	bool bitmap_mode_ = false;
	bool display_enable_ = false;
	bool rows_25_ = false;
	int y_scroll_ = 0;

	bool is_ntsc_ = false;
	bool ted_off_ = false;
	bool multicolour_mode_ = false;
	bool columns_40_ = false;
	int x_scroll_ = 0;

	bool characters_256_ = false;
	uint16_t character_base_mask_ = 0xf800;
	uint8_t character_mask_ = 0xff;
	uint8_t inversion_mask_ = 0x00;

	// Graphics mode, summarised.
	enum class VideoMode {
		Text,
		MulticolourText,
		ExtendedColourText,
		MulticolourBitmap,
		HighResBitmap,
		Blank,
	} video_mode_ = VideoMode::Text;

	uint16_t cursor_position_ = 0;
	uint16_t character_base_ = 0;
	uint16_t video_matrix_base_ = 0;
	uint16_t bitmap_base_ = 0;

	int raster_interrupt_ = 0x1ff;
	bool raster_interrupt_done_ = false;
	bool single_clock_ = false;

	// FF06 and FF07 are easier to return if read by just keeping undecoded copies of, not reconstituting.
	uint8_t ff06_ = 0;
	uint8_t ff07_ = 0;

	// Field position.
	int horizontal_counter_ = 0;

	int vertical_counter_ = 0;
	int next_vertical_counter_ = 0;
	int video_line_ = 0;

	int eos() const {			return is_ntsc_ ? 261 : 311;	}
	int vs_start() const {		return is_ntsc_ ? 229 : 254;	}
	int vs_stop() const {		return is_ntsc_ ? 232 : 257;	}
	int vblank_start() const {	return is_ntsc_ ? 226 : 251;	}
	int vblank_stop() const {	return is_ntsc_ ? 244 : 269;	}

	bool attribute_fetch_line() const {
		return video_line_ >= 0 && video_line_ < 203;
	}
	bool bad_line() const {
		return enable_display_ && attribute_fetch_line() && ((video_line_ & 7) == y_scroll_);
	}

	// Running state that's exposed.
	uint16_t character_position_reload_ = 0;
	uint16_t character_position_ = 0;

	// Running state.
	bool wide_screen_ = false;
	bool narrow_screen_ = false;

	int vertical_sub_count_ = 0;

	bool char_pos_latch_ = false;

	bool increment_character_position_ = false;
	bool increment_video_counter_ = false;
	bool refresh_ = false;
	bool character_window_ = false;
	bool horizontal_blank_ = false;
	bool horizontal_sync_ = false;
	bool horizontal_burst_ = false;
	bool enable_display_ = false;
	bool vertical_sub_active_ = false;	// Indicates the the 3-bit row counter is active.
	bool video_shift_ = false;			// Indicates that the shift register is shifting.

	bool dma_window_ = false;			// Indicates when RDY might be asserted.
	bool external_fetch_ = false;		// Covers the entire region during which the CPU is slowed down
										// to single-clock speed to allow for CPU-interleaved fetches.
	bool bad_line2_ = false;			// High for the second (i.e. character-fetch) badline.
										// Cf. bad_line() which indicates the first (i.e. attribute-fetch) badline.
	bool character_fetch_ = false;		// High for the entire region of a frame during which characters might be
										// fetched, i.e. from the first bad_line2_ until the end of the visible area.

	bool vertical_sync_ = false;
	bool vertical_screen_ = false;
	bool vertical_blank_ = false;

	int flash_count_ = 0;
	uint8_t flash_mask_ = 0xff;

	uint16_t video_counter_ = 0;
	uint16_t video_counter_reload_ = 0;

	enum class OutputState {
		Blank,
		Sync,
		Burst,
		Border,
		Pixels,
	} output_state_ = OutputState::Blank;
	int time_in_state_ = 0;
	uint16_t *pixels_ = nullptr;

	std::array<uint16_t, 5> background_{};
	std::array<uint8_t, 5> raw_background_{};

	const Commodore::Plus4::Pager &pager_;
	Interrupts &interrupts_;

	uint16_t colour(uint8_t chrominance, uint8_t luminance) const {
		// The following aren't accurate; they're eyeballed to be close enough for now in PAL.
		static constexpr uint8_t chrominances[] = {
			0xff,	0xff,
			90,		23,		105,	59,
			14,		69,		83,		78,
			50,		96,		32,		9,
			5,		41,
		};

		luminance = chrominance ? uint8_t(
			(luminance << 5) | (luminance << 2) | (luminance >> 1)
		) : 0;
		return uint16_t(
			luminance | (chrominances[chrominance] << 8)
		);
	}

	uint16_t colour(uint8_t value) const {
		return colour(value & 0x0f, (value >> 4) & 7);
	}

	/// Maintains two 320-bit shift registers, one for attributes and one for characters.
	/// Values come out of here and go through another 16-bit shift register before eventually reaching the display.
	struct ShiftLine {
	public:
		template<int channel>
		uint8_t read() const {
			return data_[channel][cursor_];
		}
		template<int channel>
		void write(uint8_t value) {
			data_[channel][cursor_] = value;
		}
		void advance() {
			++cursor_;
			if(cursor_ == 40) cursor_ = 0;
		}

	private:
		uint8_t data_[2][40];
		int cursor_ = 0;
	};
	ShiftLine shifter_;

	/// Maintains a single 32-bit shift register, which shifts in whole-byte increments with
	/// a template-provided delay time.
	template <int cycles_delay>
	struct ShiftRegister {
	public:
		uint8_t read() const {
			return uint8_t(data_);
		}
		void write(uint8_t value) {
			data_ |= uint32_t(value) << (cycles_delay * 8);
		}
		void advance() {
			data_ >>= 8;
		}

	private:
		uint32_t data_;
		static_assert(cycles_delay < sizeof(data_));
	};
	ShiftRegister<3> next_attribute_;
	ShiftRegister<3> next_character_;
	ShiftRegister<3> next_pixels_;

	/// Maintains a 16-bit pixel shift register along with a hard-switchover
	/// set of attributes.
	struct OutputSegment {
	public:
		void advance_pixels(int distance) {
			pixels_ <<= distance;
		}
		void load_pixels(uint8_t source, int offset) {
			const auto shift = 8 - offset;
			pixels_ &= ~(0xff << shift);
			pixels_ |= source << shift;
		}
		uint8_t pixels() const {
			return uint8_t(pixels_ >> 8);
		}

		template <int index>
		void set_attributes(uint8_t attributes) {
			attributes_[index] = attributes;
		}
		template <int index>
		uint8_t attributes() const {
			return attributes_[index];
		}

		void reset() {
			pixels_ = 0;
			attributes_[0] = attributes_[1] = 0;
		}

	private:
		uint16_t pixels_;
		uint8_t attributes_[2];
	};
	OutputSegment output_;

	// List of counter-triggered events.
	enum class HorizontalEvent: unsigned int {
		Begin40Columns = 0,
		Begin38Columns = 8,
		LatchCharacterPosition = 288,
		DMAWindowEnd = 295,
		EndExternalFetchWindow = 296,
		EndCharacterFetchWindow = 304,
		End38Columns = 312,
		End40Columns = 320,
		EndRefresh = 336,
		IncrementFlashCounter = 348,
		BeginBlank = 353,
		BeginSync = 359,
		VerticalSubActive = 380,
		EndOfScreen = 384,
		EndSync = 391,
		IncrementVerticalSub = 392,
		BeginExternalFetchClock = 400,
		BeginAttributeFetch = 407,
		EndBlank = 423,
		IncrementVideoCounter = 432,
		BeginShiftRegister = 440,
		ScheduleCounterReset = 455,
		CounterOverflow = 512,
	};

	// List of events that occur at a certain latency.
	enum class DelayedEvent {
		Latch = 0x01,
		Flash = 0x02,
		IncrementVerticalSub = 0x04,
		IncrementVerticalLine = 0x08,
		CounterReset = 0x10,
		UpdateDMAState = 0x20,

		Mask = CounterReset | IncrementVerticalLine | IncrementVerticalSub | Flash | Latch | UpdateDMAState,
	};
	static constexpr int DelayEventSize = 6;
	uint64_t delayed_events_ = 0;

	/// Scheudles @c event to occur after @c latency pixel-clock cycles.
	template <int latency>
	void schedule(DelayedEvent event) {
		static_assert(latency <= sizeof(delayed_events_) * 8 / DelayEventSize);
		delayed_events_ |= uint64_t(event) << (DelayEventSize * latency);
	}

	// DMA states.
	enum class DMAState {
		IDLE,
		THALT1, THALT2, THALT3, TDMA,
	} dma_state_ = DMAState::IDLE;

	//
	// Various pixel outputters.
	//
	template <int scroll>
	void draw() {
		// Bake in the video mode.
		switch(video_mode_) {
			case VideoMode::Text:				draw<scroll, VideoMode::Text>();				break;
			case VideoMode::MulticolourText:	draw<scroll, VideoMode::MulticolourText>();		break;
			case VideoMode::ExtendedColourText:	draw<scroll, VideoMode::ExtendedColourText>();	break;
			case VideoMode::MulticolourBitmap:	draw<scroll, VideoMode::MulticolourBitmap>();	break;
			case VideoMode::HighResBitmap:		draw<scroll, VideoMode::HighResBitmap>();		break;
			case VideoMode::Blank:				draw<scroll, VideoMode::Blank>();				break;
		}
	}

	template <int scroll, VideoMode mode>
	void draw() {
		// Finish off whatever is in the shifter up until the point that x position hits
		// the current scroll, then roll over on attributes and fill in the rest of the window
		// from there.
		draw_segment<scroll, mode, true>();
		output_.set_attributes<0>(next_attribute_.read());
		output_.set_attributes<1>(next_character_.read());
		draw_segment<8 - scroll, mode, false>();
	}

	template <int length, VideoMode mode, bool is_leftovers>
	void draw_segment() {
		if constexpr (length == 0) return;
		const auto target = pixels_;
		if(target) pixels_ += length;

		switch(mode) {
			case VideoMode::Text: {
				const auto attributes = output_.attributes<0>();
				const uint16_t colours[] = {
					background_[0], colour(attributes)
				};
				draw_1bpp_segment<length, true>(target, colours);
			} break;

			case VideoMode::ExtendedColourText: {
				const auto attributes = output_.attributes<0>();
				const auto character = output_.attributes<1>();
				const uint16_t colours[] = {
					background_[character >> 6],
					colour(attributes),
				};
				draw_1bpp_segment<length, false>(target, colours);
			} break;

			case VideoMode::MulticolourText: {
				const auto attributes = output_.attributes<0>();
				if(attributes & 0x08) {
					const uint16_t colours[] = {
						background_[0],
						background_[1],
						background_[2],
						colour(attributes & ~0x08),
					};
					draw_2bpp_segment<length, is_leftovers>(target, colours);
				} else {
					const uint16_t colours[] = {
						background_[0],
						colour(attributes & ~0x08),
					};
					draw_1bpp_segment<length, true>(target, colours);
				}
			} break;

			case VideoMode::HighResBitmap: {
				const auto attributes = output_.attributes<0>();
				const auto character = output_.attributes<1>();
				const uint16_t colours[] = {
					colour((character >> 0) & 0xf, (attributes >> 4) & 0x7),
					colour((character >> 4) & 0xf, (attributes >> 0) & 0x7),
				};
				draw_1bpp_segment<length, false>(target, colours);
			} break;

			case VideoMode::MulticolourBitmap: {
				const auto attributes = output_.attributes<0>();
				const auto character = output_.attributes<1>();
				const uint16_t colours[] = {
					background_[0],
					colour((character >> 4) & 0xf, (attributes >> 0) & 0x7),
					colour((character >> 0) & 0xf, (attributes >> 4) & 0x7),
					background_[1],
				};
				draw_2bpp_segment<length, is_leftovers>(target, colours);
			} break;

			case VideoMode::Blank:
				if(target) {
					std::fill(target, target + length, 0x0000);
				}
				output_.advance_pixels(length);
			break;
		}
	}

	template <int length, bool support_inversion>
	void draw_1bpp_segment(uint16_t *const target, const uint16_t *colours) {
		if(target) {
			uint8_t pixels = output_.pixels();

			if(output_.attributes<0>() & 0x80) pixels &= flash_mask_;
			if constexpr (support_inversion) {
				if(output_.attributes<1>() & 0x80) {
					pixels ^= inversion_mask_;
				}
			}

			if constexpr (length >= 1) target[0] = (pixels & 0x80) ? colours[1] : colours[0];
			if constexpr (length >= 2) target[1] = (pixels & 0x40) ? colours[1] : colours[0];
			if constexpr (length >= 3) target[2] = (pixels & 0x20) ? colours[1] : colours[0];
			if constexpr (length >= 4) target[3] = (pixels & 0x10) ? colours[1] : colours[0];
			if constexpr (length >= 5) target[4] = (pixels & 0x08) ? colours[1] : colours[0];
			if constexpr (length >= 6) target[5] = (pixels & 0x04) ? colours[1] : colours[0];
			if constexpr (length >= 7) target[6] = (pixels & 0x02) ? colours[1] : colours[0];
			if constexpr (length >= 8) target[7] = (pixels & 0x01) ? colours[1] : colours[0];
		}

		output_.advance_pixels(length);
	}

	template <int length, bool is_leftovers>
	void draw_2bpp_segment(uint16_t *const target, const uint16_t *colours) {
		constexpr int leftover = is_leftovers && (length & 1);
		static_assert(length + leftover <= 8);
		if(target) {
			const auto pixels = output_.pixels();
			// Intention: skip first output if leftover is 1, but still do the correct
			// length of output.
			if constexpr (!leftover && length >= 1) target[0] = colours[(pixels >> 6) & 3];
			if constexpr (length + leftover >= 2) target[1 - leftover] = colours[(pixels >> 6) & 3];
			if constexpr (length + leftover >= 3) target[2 - leftover] = colours[(pixels >> 4) & 3];
			if constexpr (length + leftover >= 4) target[3 - leftover] = colours[(pixels >> 4) & 3];
			if constexpr (length + leftover >= 5) target[4 - leftover] = colours[(pixels >> 2) & 3];
			if constexpr (length + leftover >= 6) target[5 - leftover] = colours[(pixels >> 2) & 3];
			if constexpr (length + leftover >= 7) target[6 - leftover] = colours[(pixels >> 0) & 3];
			if constexpr (length + leftover >= 8) target[7 - leftover] = colours[(pixels >> 0) & 3];
		}

		if constexpr (is_leftovers) {
			constexpr int shift_distance = length + leftover;
			static_assert(!(shift_distance&1));
			output_.advance_pixels(shift_distance);
		} else {
			output_.advance_pixels(length & ~1);
		}
	}
};

}
