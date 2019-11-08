//
//  Video.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/10/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef Atari_ST_Video_hpp
#define Atari_ST_Video_hpp

#include "../../Outputs/CRT/CRT.hpp"
#include "../../ClockReceiver/ClockReceiver.hpp"

namespace Atari {
namespace ST {

enum class FieldFrequency {
	Fifty = 0, Sixty = 1, SeventyTwo = 2
};

class Video {
	public:
		Video();

		/*!
			Sets the target device for video data.
		*/
		void set_scan_target(Outputs::Display::ScanTarget *scan_target);

		/*!
			Produces the next @c duration period of pixels.
		*/
		void run_for(HalfCycles duration);

		/*!
			@returns the number of cycles until there is next a change in the hsync,
			vsync or display_enable outputs.
		*/
		HalfCycles get_next_sequence_point();

		bool hsync();
		bool vsync();
		bool display_enabled();

		void set_ram(uint16_t *, size_t size);

		uint16_t read(int address);
		void write(int address, uint16_t value);

	private:
		Outputs::CRT::CRT crt_;

		uint16_t palette_[16];
		int base_address_ = 0;
		int current_address_ = 0;

		uint16_t *ram_;
		uint16_t line_buffer_[256];

		int x = 0, y = 0;
		void output_border(int duration);

		uint16_t video_mode_ = 0;
		uint16_t sync_mode_ = 0;

		FieldFrequency field_frequency_ = FieldFrequency::Fifty;
		enum class OutputBpp {
			One, Two, Four
		} output_bpp_;
		void update_output_mode();

		struct State {
			bool enable = false;
			bool blank = false;
			bool sync = false;
		} horizontal_, vertical_;
		int line_length_ = 512;

		int data_latch_position_ = 0;
		uint16_t data_latch_[4];
		union {
			uint64_t output_shifter_;
			uint32_t shifter_halves_[2];
		};
		void shift_out(int length);
		void latch_word();

		struct PixelBufferState {
			uint16_t *pixel_pointer;
			int pixels_output = 0;
			int cycles_output = 0;
			OutputBpp output_bpp;
			void flush(Outputs::CRT::CRT &crt) {
				if(cycles_output) crt.output_data(cycles_output, size_t(pixels_output));
				pixels_output = cycles_output = 0;
				pixel_pointer = nullptr;
			}
			void allocate(Outputs::CRT::CRT &crt) {
				flush(crt);
				pixel_pointer = reinterpret_cast<uint16_t *>(crt.begin_data(328));
			}
		} pixel_buffer_;
};

}
}

#endif /* Atari_ST_Video_hpp */
