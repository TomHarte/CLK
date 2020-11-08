//
//  Video.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 31/10/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#ifndef Apple_IIgs_Video_hpp
#define Apple_IIgs_Video_hpp

#include "../AppleII/VideoSwitches.hpp"
#include "../../../Outputs/CRT/CRT.hpp"
#include "../../../ClockReceiver/ClockReceiver.hpp"

namespace Apple {
namespace IIgs {
namespace Video {

/*!
	Provides IIgs video output; assumed clocking here is twice the usual Apple II clock.
	So it'll produce a single line of video every 131 cycles — 65*2 + 1, allowing for the
	stretched cycle.
*/
class VideoBase: public Apple::II::VideoSwitches<Cycles> {
	public:
		VideoBase();
		void set_internal_ram(const uint8_t *);

		bool get_is_vertical_blank();

		void set_new_video(uint8_t);
		uint8_t get_new_video();

		void clear_interrupts(uint8_t);
		uint8_t get_interrupt_register();
		void set_interrupt_register(uint8_t);

		void notify_clock_tick();

		void set_border_colour(uint8_t);
		void set_text_colour(uint8_t);

		/// Sets the scan target.
		void set_scan_target(Outputs::Display::ScanTarget *scan_target);

		/// Gets the current scan status.
		Outputs::Display::ScanStatus get_scaled_scan_status() const;

		/// Sets the type of output.
		void set_display_type(Outputs::Display::DisplayType);

		/// Gets the type of output.
		Outputs::Display::DisplayType get_display_type() const;

	private:
		Outputs::CRT::CRT crt_;

		void advance(Cycles);

		uint8_t new_video_ = 0x01;
		uint8_t interrupts_ = 0x00;
		void set_interrupts(uint8_t);

		int cycles_into_frame_ = 0;
		const uint8_t *ram_ = nullptr;

		// The modal colours.
		uint16_t border_colour_ = 0;
		uint16_t text_colour_ = 0xfff;
		uint16_t background_colour_ = 0;

		// Current pixel output buffer.
		uint16_t *pixels_ = nullptr, *next_pixel_ = nullptr;

		void output_row(int row, int start, int end);
};

class Video: public VideoBase {
	public:
		using VideoBase::VideoBase;
};

}
}
}

#endif /* Video_hpp */
