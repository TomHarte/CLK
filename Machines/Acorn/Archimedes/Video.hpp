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

#include <cstdint>

namespace Archimedes {

template <typename InterruptObserverT, typename SoundT>
struct Video {
	Video(InterruptObserverT &observer, SoundT &sound, const uint8_t *ram) :
		observer_(observer),
		sound_(sound),
		ram_(ram),
		crt_(Outputs::Display::InputDataType::Red4Green4Blue4) {
		set_clock_divider(2);
	}

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
				logger.error().append("TODO: Video palette logical colour %d to %03x", (target >> 2), value & 0x1fff);
			break;

			case 0x40:
				logger.error().append("TODO: Video border colour to %03x", value & 0x1fff);
			break;

			case 0x44:	case 0x48:	case 0x4c:
				logger.error().append("TODO: Cursor colour %d to %03x", (target - 0x44) >> 2, value & 0x1fff);
			break;

			case 0x80:
				logger.error().append("TODO: Video horizontal period: %d", (value >> 14) & 0x3ff);
				horizontal_timing_.period = timing_value(value);
			break;
			case 0x84:
				logger.error().append("TODO: Video horizontal sync width: %d", (value >> 14) & 0x3ff);
				horizontal_timing_.sync_width = timing_value(value);
			break;
			case 0x88:
				logger.error().append("TODO: Video horizontal border start: %d", (value >> 14) & 0x3ff);
				horizontal_timing_.border_start = timing_value(value);
			break;
			case 0x8c:
				logger.error().append("TODO: Video horizontal display start: %d", (value >> 14) & 0x3ff);
				horizontal_timing_.display_start = timing_value(value);
			break;
			case 0x90:
				logger.error().append("TODO: Video horizontal display end: %d", (value >> 14) & 0x3ff);
				horizontal_timing_.display_end = timing_value(value);
			break;
			case 0x94:
				logger.error().append("TODO: Video horizontal border end: %d", (value >> 14) & 0x3ff);
				horizontal_timing_.border_end = timing_value(value);
			break;
			case 0x98:
				logger.error().append("TODO: Video horizontal cursor end: %d", (value >> 14) & 0x3ff);
				horizontal_timing_.cursor_end = timing_value(value);
			break;
			case 0x9c:
				logger.error().append("TODO: Video horizontal interlace: %d", (value >> 14) & 0x3ff);
			break;

			case 0xa0:
				logger.error().append("TODO: Video vertical period: %d", (value >> 14) & 0x3ff);
				vertical_timing_.period = timing_value(value);
			break;
			case 0xa4:
				logger.error().append("TODO: Video vertical sync width: %d", (value >> 14) & 0x3ff);
				vertical_timing_.sync_width = timing_value(value);
			break;
			case 0xa8:
				logger.error().append("TODO: Video vertical border start: %d", (value >> 14) & 0x3ff);
				vertical_timing_.border_start = timing_value(value);
			break;
			case 0xac:
				logger.error().append("TODO: Video vertical display start: %d", (value >> 14) & 0x3ff);
				vertical_timing_.display_start = timing_value(value);
			break;
			case 0xb0:
				logger.error().append("TODO: Video vertical display end: %d", (value >> 14) & 0x3ff);
				vertical_timing_.display_end = timing_value(value);
			break;
			case 0xb4:
				logger.error().append("TODO: Video vertical border end: %d", (value >> 14) & 0x3ff);
				vertical_timing_.border_end = timing_value(value);
			break;
			case 0xb8:
				logger.error().append("TODO: Video vertical cursor start: %d", (value >> 14) & 0x3ff);
				vertical_timing_.cursor_start = timing_value(value);
			break;
			case 0xbc:
				logger.error().append("TODO: Video vertical cursor end: %d", (value >> 14) & 0x3ff);
				vertical_timing_.cursor_end = timing_value(value);
			break;

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
		horizontal_state_.increment_position(clock_divider_);
		horizontal_state_.phase =
			horizontal_timing_.phase_after(
				horizontal_state_.position,
				horizontal_state_.phase,
				clock_divider_);

		if(horizontal_state_.position == horizontal_timing_.period * clock_divider_) {
			horizontal_state_.position = 0;

			vertical_state_.increment_position(1);
			vertical_state_.phase =
				vertical_timing_.phase_after(
					vertical_state_.position,
					vertical_state_.phase,
					1);

			if(vertical_state_.position == vertical_timing_.period) {
				vertical_state_.position = 0;
			}
		}

		// Accumulate total phase.
		++time_in_phase_;
		Phase new_phase;
		switch(vertical_state_.phase) {
			case Phase::Sync:	new_phase = Phase::Sync;	break;
			case Phase::Blank:	new_phase = Phase::Blank;	break;
			case Phase::Border:
				new_phase = horizontal_state_.phase == Phase::Display ? Phase::Border : horizontal_state_.phase;
			break;
			case Phase::Display:
				new_phase = horizontal_state_.phase;
			break;
		}

		// Possibly output something. TODO: with actual pixels.
		if(new_phase != phase_) {
			const auto duration = static_cast<int>(time_in_phase_ / clock_divider_);

			switch(phase_) {
				case Phase::Sync:	crt_.output_sync(duration);		break;
				case Phase::Blank:	crt_.output_blank(duration);	break;
				case Phase::Display:	// TODO: pixels.
					crt_.output_level<uint16_t>(duration, 0xf888);
				break;
				case Phase::Border:
					crt_.output_level<uint16_t>(duration, 0xffff);
				break;
			}

			phase_ = new_phase;
			time_in_phase_ = 0;
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

private:
	Log::Logger<Log::Source::ARMIOC> logger;
	InterruptObserverT &observer_;
	SoundT &sound_;

	// In the current version of this code, video DMA occurrs costlessly,
	// being deferred to the component itself.
	const uint8_t *ram_ = nullptr;
	Outputs::CRT::CRT crt_;

	// Current video state.
	enum class Phase {
		Sync, Blank, Border, Display,
	};
	struct State {
		uint32_t position = 0;
		Phase phase = Phase::Sync;

		void increment_position(uint32_t divider) {
			++position;
			if(position == 1024 * divider) position = 0;
		}
	};
	State horizontal_state_, vertical_state_;
	Phase phase_ = Phase::Sync;
	uint32_t time_in_phase_ = 0;

	// Addresses.
	uint32_t buffer_start_;
	uint32_t buffer_end_;
	uint32_t frame_start_;
	uint32_t cursor_start_;

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

		Phase phase_after(uint32_t position, Phase current_phase, uint32_t divider) {
			if(position == sync_width * divider) return Phase::Blank;
			if(position == border_start * divider) return Phase::Border;
			if(position == display_start * divider) return Phase::Display;
			if(position == display_end * divider) return Phase::Border;
			if(position == border_end * divider) return Phase::Blank;
			if(position == period * divider) return Phase::Sync;
			return current_phase;
		}
	};
	Timing horizontal_timing_, vertical_timing_;

	// An interrupt flag; more closely related to the interface by which
	// my implementation of the IOC picks up an interrupt request than
	// to hardware.
	bool entered_sync_ = false;

	// The divider that would need to be applied to a 24Mhz clock to
	// get half the current pixel clock; counting is in units of half
	// the pixel clock because that's the fidelity at which the programmer
	// places horizontal events — display start, end, sync period, etc.
	uint32_t clock_divider_ = 0;

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
	}
};

}
