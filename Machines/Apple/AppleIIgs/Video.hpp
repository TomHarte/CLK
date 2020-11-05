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
#include "../../../ClockReceiver/ClockReceiver.hpp"

namespace Apple {
namespace IIgs {
namespace Video {

class VideoBase: public Apple::II::VideoSwitches<Cycles> {
	public:
		VideoBase();
		void set_internal_ram(const uint8_t *);

		void run_for(Cycles);
		bool get_is_vertical_blank();

		void set_new_video(uint8_t);
		uint8_t get_new_video();

		void clear_interrupts(uint8_t);
		uint8_t get_interrupt_register();
		void set_interrupt_register(uint8_t);

		void notify_clock_tick();

	private:
		void did_set_annunciator_3(bool) override;
		void did_set_alternative_character_set(bool) override;

		uint8_t new_video_ = 0x01;
		uint8_t interrupts_ = 0x00;
		void set_interrupts(uint8_t);

		int cycles_into_frame_ = 0;
		const uint8_t *ram_ = nullptr;
};

class Video: public VideoBase {
	public:
		using VideoBase::VideoBase;
};

}
}
}

#endif /* Video_hpp */
