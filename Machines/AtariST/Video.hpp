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

		void set_ram(uint16_t *);

		uint8_t read(int address);
		void write(int address, uint16_t value);

	private:
		Outputs::CRT::CRT crt_;

		uint16_t palette_[16];
		int base_address_ = 0;
		int current_address_ = 0;

		uint16_t *ram_;
		uint16_t line_buffer_[256];
		uint16_t *pixel_pointer_;

		int x = 0, y = 0;
		void output_border(int duration);
};

}
}

#endif /* Atari_ST_Video_hpp */
