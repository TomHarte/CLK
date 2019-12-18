//
//  Video.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/05/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef Video_hpp
#define Video_hpp

#include "../../../Outputs/CRT/CRT.hpp"
#include "../../../ClockReceiver/ClockReceiver.hpp"
#include "DeferredAudio.hpp"
#include "DriveSpeedAccumulator.hpp"

namespace Apple {
namespace Macintosh {

static const HalfCycles line_length(704);
static const int number_of_lines = 370;
static const HalfCycles frame_length(line_length * HalfCycles(number_of_lines));
static const int sync_start = 36;
static const int sync_end = 38;

/*!
	Models the 68000-era Macintosh video hardware, producing a 512x348 pixel image,
	within a total scanning area of 370 lines, at 352 cycles per line.

	This class also collects audio and 400kb drive-speed data, forwarding those values.
*/
class Video {
	public:
		/*!
			Constructs an instance of @c Video sourcing its pixel data from @c ram and
			providing audio and drive-speed bytes to @c audio and @c drive_speed_accumulator.
		*/
		Video(DeferredAudio &audio, DriveSpeedAccumulator &drive_speed_accumulator);

		/*!
			Sets the target device for video data.
		*/
		void set_scan_target(Outputs::Display::ScanTarget *scan_target);

		/*!
			Produces the next @c duration period of pixels.
		*/
		void run_for(HalfCycles duration);

		/*!
			Sets whether the alternate screen and/or audio buffers should be used to source data.
		*/
		void set_use_alternate_buffers(bool use_alternate_screen_buffer, bool use_alternate_audio_buffer);

		/*!
			Provides a base address and a mask indicating which parts of the generated video and audio/drive addresses are
			actually decoded, accessing *word-sized memory*; e.g. for a 128kb Macintosh this should be (1 << 16) - 1 = 0xffff.
		*/
		void set_ram(uint16_t *ram, uint32_t mask);

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
			const int column = int((offset_position % line_length).as_integral()) >> 4;
			const int line = int((offset_position / line_length).as_integral());
			return line < 342 && column < 32;
		}

		/*!
			@returns the amount of time until there is next a transition on the
				vsync signal.
		*/
		HalfCycles get_next_sequence_point();

	private:
		DeferredAudio &audio_;
		DriveSpeedAccumulator &drive_speed_accumulator_;

		Outputs::CRT::CRT crt_;
		uint16_t *ram_ = nullptr;
		uint32_t ram_mask_ = 0;

		HalfCycles frame_position_;

		size_t video_address_ = 0;
		size_t audio_address_ = 0;

		uint8_t *pixel_buffer_ = nullptr;

		bool use_alternate_screen_buffer_ = false;
		bool use_alternate_audio_buffer_ = false;
};

}
}

#endif /* Video_hpp */
