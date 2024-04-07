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
		crt_.set_visible_area(Outputs::Display::Rect(0.06f, 0.07f, 0.9f, 0.9f));
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
				logger.error().append("TODO: video control: %08x", value);

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
				sound_.set_frequency(value & 0x7f);
			break;

			default:
				logger.error().append("TODO: unrecognised VIDC write of %08x", value);
			break;
		}
	}

	void tick() {
		// Pick new horizontal state, possibly rolling over into the vertical.
		horizontal_state_.increment_position(horizontal_timing_);

		if(horizontal_state_.position == horizontal_timing_.period) {
			horizontal_state_.position = 0;
			vertical_state_.increment_position(vertical_timing_);
			pixel_count_ = 0;

			if(vertical_state_.position == vertical_timing_.period) {
				vertical_state_.position = 0;
				address_ = frame_start_;
				cursor_address_ = cursor_start_;
				entered_sync_ = true;
				interrupt_observer_.update_interrupts();
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

		// Update cursor pixel counter if applicable; this might mean triggering it
		// and it might just mean advancing it if it has already been triggered.
		if(vertical_state_.cursor_active) {
			const auto pixel_position = horizontal_state_.position << 1;
			if(pixel_position <= horizontal_timing_.cursor_start && (pixel_position + 2) > horizontal_timing_.cursor_start) {
				cursor_pixel_ = int(horizontal_timing_.cursor_start) - int(pixel_position);
			}
		}

		// Grab some more pixels if appropriate.
		const auto flush_pixels = [&]() {
			const auto duration = static_cast<int>(time_in_phase_);
			crt_.output_data(duration, static_cast<size_t>(time_in_phase_) * 2);
			time_in_phase_ = 0;
			pixels_ = nullptr;
		};

		if(phase_ == Phase::Display) {
			if(pixels_ && time_in_phase_ == PixelBufferSize/2) {
				flush_pixels();
			}

			if(!pixels_) {
				if(time_in_phase_) {
					flush_pixels();
				}

				pixels_ = reinterpret_cast<uint16_t *>(crt_.begin_data(PixelBufferSize));
			}

			const auto next_byte = [&]() -> uint8_t {
				const auto next = ram_[address_];
				++address_;

				// `buffer_end_` is the final address that a 16-byte block will be fetched from;
				// the +16 here papers over the fact that I'm not accurately implementing DMA.
				if(address_ == buffer_end_ + 16) {
					address_ = buffer_start_;
				}
				return next;
			};

			if(pixels_) {
				// Each tick in here is two ticks of the pixel clock, so:
				//
				//	8bpp mode: output two bytes;
				//	4bpp mode: output one byte;
				//	2bpp mode: output one byte every second tick;
				//	1bpp mode: output one byte every fourth tick.
				switch(colour_depth_) {
					case Depth::EightBPP: {
						uint8_t next = next_byte();
						pixels_[0] = (colours_[next & 0xf] & colour(0b0111'0011'0111)) | high_spread[next >> 4];

						next = next_byte();
						pixels_[1] = (colours_[next & 0xf] & colour(0b0111'0011'0111)) | high_spread[next >> 4];
					} break;

					case Depth::FourBPP: {
						const uint8_t next = next_byte();

						pixels_[0] = colours_[next & 0xf];
						pixels_[1] = colours_[next >> 4];
					} break;

					case Depth::TwoBPP: {
						if(!(pixel_count_&1)) {
							pixel_data_ = next_byte();
						}

						pixels_[0] = colours_[pixel_data_ & 3];
						pixels_[1] = colours_[(pixel_data_ >> 2) & 3];
						pixel_data_ >>= 4;
					} break;

					case Depth::OneBPP: {
						if(!(pixel_count_&3)) {
							pixel_data_ = next_byte();
						}

						pixels_[0] = colours_[pixel_data_ & 1];
						pixels_[1] = colours_[(pixel_data_ >> 1) & 1];
						pixel_data_ >>= 2;
					} break;
				}

				// Overlay cursor if applicable.
				// TODO: have all BPP modes output only two pixels at a time, and pull this out of the loop.
				// TODO: pull this so far out that the cursor can display over the border, too.
				if(cursor_pixel_ < 32) {
					if(cursor_pixel_ >= 0) {
						const auto pixel = cursor_image_[static_cast<size_t>(cursor_pixel_)];
						if(pixel) {
							pixels_[0] = cursor_colours_[pixel];
						}
					}
					if(cursor_pixel_ < 31) {
						const auto pixel = cursor_image_[static_cast<size_t>(cursor_pixel_ + 1)];
						if(pixel) {
							pixels_[1] = cursor_colours_[pixel];
						}
					}
					cursor_pixel_ += 2;
				}

				pixels_ += 2;
			} else {
				// TODO: don't assume 4bpp here either.
				switch(colour_depth_) {
					case Depth::EightBPP:
						next_byte();
						next_byte();
					break;
					case Depth::FourBPP:
						next_byte();
					break;
					case Depth::TwoBPP:
						if(!(pixel_count_&1)) {
							next_byte();
						}
					break;
					case Depth::OneBPP:
						if(!(pixel_count_&3)) {
							next_byte();
						}
					break;
				}
			}

			++pixel_count_;
		}

		// Accumulate total phase.
		++time_in_phase_;

		// Determine current output phase.
		Phase new_phase;
		switch(vertical_state_.phase()) {
			case Phase::Sync:	new_phase = Phase::Sync;	break;
			case Phase::Blank:	new_phase = Phase::Blank;	break;
			case Phase::Border:
				new_phase = horizontal_state_.phase() == Phase::Display ? Phase::Border : horizontal_state_.phase();
			break;
			case Phase::Display:
				new_phase = horizontal_state_.phase();
			break;
		}

		// Possibly output something.
		if(new_phase != phase_) {
			if(time_in_phase_) {
				const auto duration = static_cast<int>(time_in_phase_);

				switch(phase_) {
					case Phase::Sync:		crt_.output_sync(duration);									break;
					case Phase::Blank:		crt_.output_blank(duration);								break;
					case Phase::Display:	flush_pixels();												break;
					case Phase::Border:		crt_.output_level<uint16_t>(duration, border_colour_);		break;
				}
				time_in_phase_ = 0;
			}

			phase_ = new_phase;
		}
	}

	/// @returns @c true if a vertical retrace interrupt has been signalled since the last call to @c interrupt(); @c false otherwise.
	bool interrupt() {
		// Guess: edge triggered?
		const bool interrupt = entered_sync_;
		entered_sync_ = false;
		return interrupt;
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

	// Current video state.
	enum class Phase {
		Sync, Blank, Border, Display,
	};
	struct State {
		uint32_t position = 0;

		void increment_position(const Timing &timing) {
			++position;
			if(position == 1024) position = 0;

			if(position == timing.period) {
				sync_active = timing.sync_width;
				display_started = !timing.display_start;
				display_ended = !timing.display_end;
				border_started = !timing.border_start;
				border_ended = !timing.border_end;
				cursor_active = !timing.cursor_start;
			} else {
				sync_active &= position != timing.sync_width;
				display_started |= position == timing.display_start;
				display_ended |= position == timing.display_end;
				border_started |= position == timing.border_start;
				border_ended |= position == timing.border_end;

				cursor_active |= position == timing.cursor_start;
				cursor_active &= position != timing.cursor_end;
			}
		}

		bool sync_active = true;
		bool border_started = false;
		bool border_ended = false;
		bool display_started = false;
		bool display_ended = false;
		bool cursor_active = false;

		Phase phase() const {
			if(sync_active) return Phase::Sync;
			if(display_started && !display_ended) return Phase::Display;
			if(border_started && !border_ended) return Phase::Border;
			return Phase::Blank;
		}
	};
	State horizontal_state_, vertical_state_;
	Phase phase_ = Phase::Sync;
	uint32_t time_in_phase_ = 0;
	uint32_t pixel_count_ = 0;
	uint16_t *pixels_ = nullptr;

	// It is elsewhere assumed that this size is a multiple of 8.
	static constexpr size_t PixelBufferSize = 320;

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

	// Ephemeral graphics data.
	uint8_t pixel_data_ = 0;

	// Colour palette, converted to internal format.
	uint16_t border_colour_;
	std::array<uint16_t, 16> colours_{};
	std::array<uint16_t, 4> cursor_colours_{};

	// An interrupt flag; more closely related to the interface by which
	// my implementation of the IOC picks up an interrupt request than
	// to hardware.
	bool entered_sync_ = false;

	// The divider that would need to be applied to a 24Mhz clock to
	// get half the current pixel clock; counting is in units of half
	// the pixel clock because that's the fidelity at which the programmer
	// places horizontal events — display start, end, sync period, etc.
	uint32_t clock_divider_ = 0;

	enum class Depth {
		OneBPP = 0b00,
		TwoBPP = 0b01,
		FourBPP = 0b10,
		EightBPP = 0b11,
	} colour_depth_;

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
};

}
