//
//  Video.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 12/10/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Video_hpp
#define Video_hpp

#include "../../Outputs/CRT/CRT.hpp"

namespace Oric {

class VideoOutput {
	public:
		VideoOutput(uint8_t *memory);
		std::shared_ptr<Outputs::CRT::CRT> get_crt();
		void run_for_cycles(int number_of_cycles);

	private:
		uint8_t *_ram;
		std::shared_ptr<Outputs::CRT::CRT> _crt;

		// Counters
		int _counter, _frame_counter;

		// Output state
		enum State {
			Blank, Sync, Pixels
		} _state;
		unsigned int _cycles_in_state;
		uint8_t *_pixel_target;

		// Registers
		uint8_t _ink, _style, _paper;
		bool _is_graphics_mode;
};

}

#endif /* Video_hpp */
