//
//  Video.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/04/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#ifndef Video_hpp
#define Video_hpp

#include "../../Outputs/CRT/CRT.hpp"
#include "../../ClockReceiver/ClockReceiver.hpp"

namespace AppleII {

class Video {
	public:
		/// Constructs an instance of the video feed; a CRT is also created.
		Video();
		/// @returns The CRT this video feed is feeding.
		Outputs::CRT::CRT *get_crt();

		/*!
			Advances time by @c cycles; expects to be fed by the CPU clock.
			Implicitly adds an extra half a colour clock at the end of every
			line.
		*/
		void run_for(const Cycles);

		// Inputs for the various soft switches.
		void set_graphics_mode();
		void set_text_mode();
		void set_mixed_mode(bool);
		void set_video_page(int);
		void set_low_resolution();
		void set_high_resolution();

	private:
		std::unique_ptr<Outputs::CRT::CRT> crt_;

		int video_page_ = 0;
		int row_ = 0, column_ = 0;
		uint8_t *pixel_pointer_ = nullptr;
};

}

#endif /* Video_hpp */
