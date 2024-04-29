//
//  Video.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/03/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#pragma once

#include "../../../Outputs/Log.hpp"
#include "../../../Outputs/CRT/CRT.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>

namespace Archimedes {

template <typename InterruptObserverT, typename ClockRateObserverT, typename SoundT>
struct Video {
	Video(InterruptObserverT &interrupt_observer, ClockRateObserverT &clock_rate_observer, SoundT &sound, const uint8_t *ram) :
		interrupt_observer_(interrupt_observer),
		clock_rate_observer_(clock_rate_observer),
		sound_(sound),
		ram_(ram),
		crt_(Outputs::Display::InputDataType::Red4Green4Blue4) {
		set_clock_divider(3);
		crt_.set_visible_area(Outputs::Display::Rect(0.041f, 0.04f, 0.95f, 0.95f));
		crt_.set_display_type(Outputs::Display::DisplayType::RGB);
	}

	static constexpr uint16_t colour(uint32_t value) {
		uint8_t packed[2]{};
		packed[0] = value & 0xf;
		packed[1] = (value & 0xf0) | ((value & 0xf00) >> 8);

#if TARGET_RT_BIG_ENDIAN
		return static_cast<uint16_t>(packed[1] | (packed[0] << 8));
#else
		return static_cast<uint16_t>(packed[0] | (packed[1] << 8));
#endif
	};
	static constexpr uint16_t high_spread[] = {
		colour(0b0000'0000'0000),	colour(0b0000'0000'1000),	colour(0b0000'0100'0000),	colour(0b0000'0100'1000),
		colour(0b0000'1000'0000),	colour(0b0000'1000'1000),	colour(0b0000'1100'0000),	colour(0b0000'1100'1000),
		colour(0b1000'0000'0000),	colour(0b1000'0000'1000),	colour(0b1000'0100'0000),	colour(0b1000'0100'1000),
		colour(0b1000'1000'0000),	colour(0b1000'1000'1000),	colour(0b1000'1100'0000),	colour(0b1000'1100'1000),
	};

	void write(uint32_t value) {
		const auto target = (value >> 24) & 0xfc;
		const auto timing_value = [](uint32_t value) -> uint32_t {
			return (value >> 14) & 0x3ff;
		};

		switch(target) {
			case 0x00:	case 0x04:	case 0x08:	case 0x0c:
			case 0x10:	case 0x14:	case 0x18:	case 0x1c:
			case 0x20:	case 0x24:	case 0x28:	case 0x2c:
			case 0x30:	case 0x34:	case 0x38:	case 0x3c:
				colours_[target >> 2] = colour(value);
			break;

			case 0x40:	border_colour_ = colour(value);	break;

			case 0x44:	case 0x48:	case 0x4c:
				cursor_colours_[(target - 0x40) >> 2] = colour(value);
			break;

			case 0x80:	horizontal_timing_.period = timing_value(value);		break;
			case 0x84:	horizontal_timing_.sync_width = timing_value(value);	break;
			case 0x88:	horizontal_timing_.border_start = timing_value(value);	break;
			case 0x8c:	horizontal_timing_.display_start = timing_value(value);	break;
			case 0x90:	horizontal_timing_.display_end = timing_value(value);	break;
			case 0x94:	horizontal_timing_.border_end = timing_value(value);	break;
			case 0x98:
				horizontal_timing_.cursor_start = (value >> 13) & 0x7ff;
				cursor_shift_ = (value >> 11) & 3;
			break;
			case 0x9c:
				logger.error().append("TODO: Video horizontal interlace: %d", (value >> 14) & 0x3ff);
			break;

			case 0xa0:	vertical_timing_.period = timing_value(value);			break;
			case 0xa4:	vertical_timing_.sync_width = timing_value(value);		break;
			case 0xa8:	vertical_timing_.border_start = timing_value(value);	break;
			case 0xac:	vertical_timing_.display_start = timing_value(value);	break;
			case 0xb0:	vertical_timing_.display_end = timing_value(value);		break;
			case 0xb4:	vertical_timing_.border_end = timing_value(value);		break;
			case 0xb8:	vertical_timing_.cursor_start = timing_value(value);	break;
			case 0xbc:	vertical_timing_.cursor_end = timing_value(value);		break;

			case 0xe0:
				// Set pixel rate. This is the value that a 24Mhz clock should be divided
				// by to get half the pixel rate.
				switch(value & 0b11) {
					case 0b00:	set_clock_divider(6);	break;	// i.e. pixel clock = 8Mhz.
					case 0b01:	set_clock_divider(4);	break;	// 12Mhz.
					case 0b10:	set_clock_divider(3);	break;	// 16Mhz.
					case 0b11:	set_clock_divider(2);	break;	// 24Mhz.
				}

				// Set colour depth.
				colour_depth_ = Depth((value >> 2) & 0b11);
			break;

			//
			// Sound parameters.
			//
			case 0x60:	case 0x64:	case 0x68:	case 0x6c:
			case 0x70:	case 0x74:	case 0x78:	case 0x7c: {
				const uint8_t channel = ((value >> 26) + 7) & 7;
				sound_.set_stereo_image(channel, value & 7);
			} break;

			case 0xc0:
				sound_.set_frequency(value & 0xff);
			break;

			default:
				logger.error().append("TODO: unrecognised VIDC write of %08x", value);
			break;
		}
	}

	void tick() {
		// Pick new horizontal state, possibly rolling over into the vertical.
		horizontal_state_.increment_position(horizontal_timing_);

		if(horizontal_state_.did_restart()) {
			end_horizontal();

			const auto old_phase = vertical_state_.phase();
			vertical_state_.increment_position(vertical_timing_);

			const auto phase = vertical_state_.phase();
			if(phase != old_phase) {
				// Copy frame and cursor start addresses into counters at the
				// start of the first vertical display line.
				if(phase == Phase::Display) {
					address_ = frame_start_;
					cursor_address_ = cursor_start_;
				}
				if(old_phase == Phase::Display) {
					entered_flyback_ = true;
					interrupt_observer_.update_interrupts();
				}
			}

			// Determine which next 8 bytes will be the cursor image for this line.
			// Pragmatically, updating cursor_address_ once per line avoids probable
			// errors in getting it to appear appropriately over both pixels and border.
			if(vertical_state_.cursor_active) {
				uint8_t *cursor_pixel = cursor_image_.data();
				for(int byte = 0; byte < 8; byte ++) {
					cursor_pixel[0] = (ram_[cursor_address_] >> 0) & 3;
					cursor_pixel[1] = (ram_[cursor_address_] >> 2) & 3;
					cursor_pixel[2] = (ram_[cursor_address_] >> 4) & 3;
					cursor_pixel[3] = (ram_[cursor_address_] >> 6) & 3;
					cursor_pixel += 4;
					++cursor_address_;
				}
			}
			cursor_pixel_ = 32;
		}

		// Fetch if relevant display signals are active.
		if(vertical_state_.display_active() && horizontal_state_.display_active()) {
			const auto next_byte = [&]() {
				const auto next = ram_[address_];
				++address_;

				// `buffer_end_` is the final address that a 16-byte block will be fetched from;
				// the +16 here papers over the fact that I'm not accurately implementing DMA.
				if(address_ == buffer_end_ + 16) {
					address_ = buffer_start_;
				}
				bitmap_queue_[bitmap_queue_pointer_ & 7] = next;
				++bitmap_queue_pointer_;
			};

			switch(colour_depth_) {
				case Depth::EightBPP:		next_byte();	next_byte();		break;
				case Depth::FourBPP:		next_byte();						break;
				case Depth::TwoBPP:			if(!(pixel_count_&3)) next_byte();	break;
				case Depth::OneBPP:			if(!(pixel_count_&7)) next_byte();	break;
			}
		}

		// Move along line.
		switch(vertical_state_.phase()) {
			case Phase::Sync:		tick_horizontal<Phase::Sync>();		break;
			case Phase::Blank:		tick_horizontal<Phase::Blank>();	break;
			case Phase::Border:		tick_horizontal<Phase::Border>();	break;
			case Phase::Display:	tick_horizontal<Phase::Display>();	break;
		}
		++time_in_phase_;
	}

	/// @returns @c true if a vertical retrace interrupt has been signalled since the last call to @c interrupt(); @c false otherwise.
	bool interrupt() {
		// Guess: edge triggered?
		const bool interrupt = entered_flyback_;
		entered_flyback_ = false;
		return interrupt;
	}

	bool flyback_active() const {
		return vertical_state_.phase() != Phase::Display;
	}

	void set_frame_start(uint32_t address) 	{	frame_start_ = address;		}
	void set_buffer_start(uint32_t address)	{	buffer_start_ = address;	}
	void set_buffer_end(uint32_t address)	{	buffer_end_ = address;		}
	void set_cursor_start(uint32_t address)	{	cursor_start_ = address;	}

	Outputs::CRT::CRT &crt() 				{ return crt_; }
	const Outputs::CRT::CRT &crt() const	{ return crt_; }

	int clock_divider() const {
		return static_cast<int>(clock_divider_);
	}

private:
	Log::Logger<Log::Source::ARMIOC> logger;
	InterruptObserverT &interrupt_observer_;
	ClockRateObserverT &clock_rate_observer_;
	SoundT &sound_;

	// In the current version of this code, video DMA occurrs costlessly,
	// being deferred to the component itself.
	const uint8_t *ram_ = nullptr;
	Outputs::CRT::CRT crt_;

	// Horizontal and vertical timing.
	struct Timing {
		uint32_t period = 0;
		uint32_t sync_width = 0;
		uint32_t border_start = 0;
		uint32_t border_end = 0;
		uint32_t display_start = 0;
		uint32_t display_end = 0;
		uint32_t cursor_start = 0;
		uint32_t cursor_end = 0;
	};
	uint32_t cursor_shift_ = 0;
	Timing horizontal_timing_, vertical_timing_;

	enum class Depth {
		OneBPP = 0b00,
		TwoBPP = 0b01,
		FourBPP = 0b10,
		EightBPP = 0b11,
	};

	// Current video state.
	enum class Phase {
		Sync, Blank, Border, Display,
	};
	template <bool is_vertical>
	struct State {
		uint32_t position = 0;
		uint32_t display_start = 0;
		uint32_t display_end = 0;

		void increment_position(const Timing &timing) {
			if(position == timing.sync_width)		state |= SyncEnded;
			if(position == timing.display_start)	{ state |= DisplayStarted; display_start = position; }
			if(position == timing.display_end)		{ state |= DisplayEnded; display_end = position; }
			if(position == timing.border_start)		state |= BorderStarted;
			if(position == timing.border_end)		state |= BorderEnded;

			cursor_active |= position == timing.cursor_start;
			cursor_active &= position != timing.cursor_end;

			if(position == timing.period) {
				state = DidRestart;
				position = 0;

				// Both display start and end need to be seeded as bigger than can be reached,
				// while having some overhead for addition.
				display_end = display_start = std::numeric_limits<uint32_t>::max() >> 1;
			} else {
				++position;
				if(position == 1024) position = 0;
			}
		}

		bool is_outputting(Depth depth) const {
			return position >= display_start + output_latencies[static_cast<uint32_t>(depth)] && position < display_end + output_latencies[static_cast<uint32_t>(depth)];
		}

		uint32_t output_cycle(Depth depth) const {
			return position - display_start - output_latencies[static_cast<uint32_t>(depth)];
		}

		static constexpr uint32_t output_latencies[] = {
			19 >> 1,		// 1 bpp.
			11 >> 1,		// 2 bpp.
			7 >> 1,			// 4 bpp.
			5 >> 1			// 8 bpp.
		};

		static constexpr uint8_t SyncEnded = 0x1;
		static constexpr uint8_t BorderStarted = 0x2;
		static constexpr uint8_t BorderEnded = 0x4;
		static constexpr uint8_t DisplayStarted = 0x8;
		static constexpr uint8_t DisplayEnded = 0x10;
		static constexpr uint8_t DidRestart = 0x20;
		uint8_t state = 0;

		bool cursor_active = false;

		bool did_restart() {
			const bool result = state & DidRestart;
			state &= ~DidRestart;
			return result;
		}

		bool display_active() const {
			return (state & DisplayStarted) && !(state & DisplayEnded);
		}

		Phase phase(Phase horizontal_fallback = Phase::Border) const {
			// TODO: turn the following logic into a lookup table.
			if(!(state & SyncEnded)) {
				return Phase::Sync;
			}
			if(!(state & BorderStarted) || (state & BorderEnded)) {
				return Phase::Blank;
			}
			if constexpr (!is_vertical) {
				return horizontal_fallback;
			}

			if(!(state & DisplayStarted) || (state & DisplayEnded)) {
				return Phase::Border;
			}
			return Phase::Display;
		}
	};
	State<false> horizontal_state_;
	State<true> vertical_state_;

	int time_in_phase_ = 0;
	Phase phase_;
	uint16_t phased_border_colour_;

	int pixel_count_ = 0;
	int display_area_start_ = 0;
	uint16_t *pixels_ = nullptr;

	// It is elsewhere assumed that this size is a multiple of 8.
	static constexpr size_t PixelBufferSize = 256;

	// Programmer-set addresses.
	uint32_t buffer_start_ = 0;
	uint32_t buffer_end_ = 0;
	uint32_t frame_start_ = 0;
	uint32_t cursor_start_ = 0;

	// Ephemeral address state.
	uint32_t address_ = 0;

	// Horizontal cursor output state.
	uint32_t cursor_address_ = 0;
	int cursor_pixel_ = 0;
	std::array<uint8_t, 32> cursor_image_;

	// Colour palette, converted to internal format.
	uint16_t border_colour_;
	std::array<uint16_t, 16> colours_{};
	std::array<uint16_t, 4> cursor_colours_{};

	// An interrupt flag; more closely related to the interface by which
	// my implementation of the IOC picks up an interrupt request than
	// to hardware.
	bool entered_flyback_ = false;

	// The divider that would need to be applied to a 24Mhz clock to
	// get half the current pixel clock; counting is in units of half
	// the pixel clock because that's the fidelity at which the programmer
	// places horizontal events — display start, end, sync period, etc.
	uint32_t clock_divider_ = 0;
	Depth colour_depth_;

	// A temporary buffer that holds video contents during the latency
	// period between their generation and their output.
	uint8_t bitmap_queue_[8];
	int bitmap_queue_pointer_ = 0;

	void set_clock_divider(uint32_t divider) {
		if(divider == clock_divider_) {
			return;
		}

		clock_divider_ = divider;
		const auto cycles_per_line = static_cast<int>(24'000'000 / (divider * 312 * 50));
		crt_.set_new_timing(
			cycles_per_line,
			312,								/* Height of display. */
			Outputs::CRT::PAL::ColourSpace,
			Outputs::CRT::PAL::ColourCycleNumerator,
			Outputs::CRT::PAL::ColourCycleDenominator,
			Outputs::CRT::PAL::VerticalSyncLength,
			Outputs::CRT::PAL::AlternatesPhase);
		clock_rate_observer_.update_clock_rates();
	}

	void flush_pixels() {
		crt_.output_data(time_in_phase_, static_cast<size_t>(pixel_count_));
		time_in_phase_ = 0;
		pixel_count_ = 0;
		pixels_ = nullptr;
	}

	void set_phase(Phase phase) {
		if(time_in_phase_) {
			switch(phase_) {
				case Phase::Sync:		crt_.output_sync(time_in_phase_);									break;
				case Phase::Blank:		crt_.output_blank(time_in_phase_);									break;
				case Phase::Border:		crt_.output_level<uint16_t>(time_in_phase_, phased_border_colour_);	break;
				case Phase::Display:	flush_pixels();														break;
			}
		}

		phase_ = phase;
		time_in_phase_ = 0;
		phased_border_colour_ = border_colour_;
		pixel_count_ = 0;
	}

	void end_horizontal() {
		set_phase(Phase::Sync);
		display_area_start_ = -1;
		bitmap_queue_pointer_ = 0;
	}

	template <Phase vertical_phase> void tick_horizontal() {
		// Sync lines: obey nothing. All sync, all the time.
		if constexpr (vertical_phase == Phase::Sync) {
			return;
		}

		// Blank lines: obey only the transition from sync to non-sync.
		if constexpr (vertical_phase == Phase::Blank) {
			if(phase_ == Phase::Sync && horizontal_state_.phase() != Phase::Sync) {
				set_phase(Phase::Blank);
			}
			return;
		}

		// Border lines: ignore display phases; also  reset the border phase if the colour changes.
		if constexpr (vertical_phase == Phase::Border) {
			const auto phase = horizontal_state_.phase(Phase::Border);
			if(phase != phase_ || (phase_ == Phase::Border && border_colour_ != phased_border_colour_)) {
				set_phase(phase);
			}
			return;
		}

		if constexpr (vertical_phase != Phase::Display) {
			// Should be impossible.
			assert(false);
		}

		// Some timing facts, to explain what would otherwise be magic constants.
		static constexpr int CursorDelay = 5;	// The cursor will appear six pixels after its programmed trigger point.
												// ... BUT! Border and display are currently a pixel early. So move the
												// cursor for alignment.

		// Deal with sync and blank via set_phase(); collapse display and border into Phase::Display.
		const auto phase = horizontal_state_.phase(Phase::Display);
		if(phase != phase_) set_phase(phase);

		// Update cursor pixel counter if applicable; this might mean triggering it
		// and it might just mean advancing it if it has already been triggered.
		cursor_pixel_ += 2;
		if(vertical_state_.cursor_active) {
			const auto pixel_position = horizontal_state_.position << 1;
			if(pixel_position <= horizontal_timing_.cursor_start && (pixel_position + 2) > horizontal_timing_.cursor_start) {
				cursor_pixel_ = int(horizontal_timing_.cursor_start) - int(pixel_position) - CursorDelay;
			}
		}

		// If this is not [collapsed] Phase::Display, just stop here.
		if(phase_ != Phase::Display) return;

		// Display phase: maintain an output buffer (if available).
		if(pixel_count_ == PixelBufferSize)	flush_pixels();
		if(!pixel_count_)					pixels_ = reinterpret_cast<uint16_t *>(crt_.begin_data(PixelBufferSize));

		// Output.
		if(pixels_) {
			// Paint the border colour for potential painting over.

			if(horizontal_state_.is_outputting(colour_depth_)) {
				const auto source = horizontal_state_.output_cycle(colour_depth_);

				// TODO: all below should be delayed an extra pixel. As should the border, actually. Fix up externally?
				switch(colour_depth_) {
					case Depth::EightBPP: {
						const uint8_t *bitmap = &bitmap_queue_[(source << 1) & 7];
						pixels_[0] = (colours_[bitmap[0] & 0xf] & colour(0b0111'0011'0111)) | high_spread[bitmap[0] >> 4];
						pixels_[1] = (colours_[bitmap[1] & 0xf] & colour(0b0111'0011'0111)) | high_spread[bitmap[1] >> 4];
					} break;

					case Depth::FourBPP:
						pixels_[0] = colours_[bitmap_queue_[source & 7] & 0xf];
						pixels_[1] = colours_[bitmap_queue_[source & 7] >> 4];
					break;

					case Depth::TwoBPP: {
						uint8_t &bitmap = bitmap_queue_[(source >> 1) & 7];
						pixels_[0] = colours_[bitmap & 3];
						pixels_[1] = colours_[(bitmap >> 2) & 3];
						bitmap >>= 4;
					} break;

					case Depth::OneBPP: {
						uint8_t &bitmap = bitmap_queue_[(source >> 2) & 7];
						pixels_[0] = colours_[bitmap & 1];
						pixels_[1] = colours_[(bitmap >> 1) & 1];
						bitmap >>= 2;
					} break;
				}
			} else {
				pixels_[0] = pixels_[1] = border_colour_;
			}

			// Overlay cursor if applicable.
			if(cursor_pixel_ < 32) {
				if(cursor_pixel_ >= 0) {
					const auto pixel = cursor_image_[static_cast<size_t>(cursor_pixel_)];
					if(pixel) {
						pixels_[0] = cursor_colours_[pixel];
					}
				}
				if(cursor_pixel_ >= -1 && cursor_pixel_ < 31) {
					const auto pixel = cursor_image_[static_cast<size_t>(cursor_pixel_ + 1)];
					if(pixel) {
						pixels_[1] = cursor_colours_[pixel];
					}
				}
			}

			pixels_ += 2;
		}

		pixel_count_ += 2;
	}
};

}
