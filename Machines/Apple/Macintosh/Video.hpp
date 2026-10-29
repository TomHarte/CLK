//
//  Video.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/05/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#pragma once

#include "ClockReceiver/ClockReceiver.hpp"
#include "Outputs/CRT/CRT.hpp"

#include "Numeric/CircularCounter.hpp"

namespace Apple::Macintosh {

constexpr HalfCycles line_length(704);
constexpr int number_of_lines = 370;
constexpr HalfCycles frame_length(line_length * HalfCycles(number_of_lines));
constexpr int sync_start = 36;
constexpr int sync_end = 38;

/*!
	In a real Macintosh the video collector is also responsible for audio collection.

	This emulator has opted to decouple video activity from audio collection, which
	means the audio collector needs to be able to learn about video phase in order to
	known _when_ to collect audio.

	The struct below captures that information.
*/
struct AudioPhase {
	// A phase of 0 = collect audio now. This should increment with the CPU clock.
	Numeric::CircularCounter<unsigned, line_length.as<int>()> sample_phase;

	// A phase of 0 = reset audio collection address. This should increment each time
	// sample phase wraps around.
	Numeric::CircularCounter<unsigned, number_of_lines> buffer_phase;
};

/*!
	Models the 68000-era Macintosh video hardware, producing a 512x348 pixel image,
	within a total scanning area of 370 lines, at 352 cycles per line.

	This class also collects audio and 400kb drive-speed data, forwarding those values.
*/
class Video {
public:
	Video();

	/*!
		Sets the target device for video data.
	*/
	void set_scan_target(Outputs::Display::ScanTarget *scan_target);

	/// Gets the current scan status.
	Outputs::Display::ScanStatus get_scaled_scan_status() const;

	/*!
		Produces the next @c duration period of pixels.
	*/
	void run_for(HalfCycles);

	/*!
		@returns The current audio phase.
	*/
	AudioPhase audio_phase() const;

	/*!
		Sets whether the alternate screen and/or audio buffers should be used to source data.
	*/
	void set_use_alternate_buffer(bool);

	/*!
		Provides a base address and a mask indicating which parts of the generated video and audio/drive addresses are
		actually decoded, accessing *word-sized memory*; e.g. for a 128kb Macintosh this should be (1 << 16) - 1 = 0xffff.
	*/
	void set_ram(const uint16_t *ram, uint32_t mask);

	/*!
		@returns @c true if the video is currently outputting a vertical sync, @c false otherwise.
	*/
	bool vsync();

	/*
		@returns @c true if in @c offset half cycles from now, the video will be outputting pixels;
			@c false otherwise.
	*/
	bool is_outputting(HalfCycles offset = HalfCycles(0)) {
		const auto offset_position = frame_position_ + offset % frame_length;
		const auto column = (offset_position % line_length).as<int>() >> 4;
		const auto line = (offset_position / line_length).as<int>();
		return line < 342 && column < 32;
	}

	/*!
		@returns the amount of time until there is next a transition on the
			vsync signal.
	*/
	HalfCycles next_sequence_point();

private:
	Outputs::CRT::CRT crt_;
	const uint16_t *ram_ = nullptr;
	uint32_t ram_mask_ = 0;

	HalfCycles frame_position_;

	size_t video_address_ = 0;

	uint64_t *pixel_buffer_ = nullptr;

	bool use_alternate_buffer_ = false;
};

}
