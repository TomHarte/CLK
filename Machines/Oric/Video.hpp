//
//  Video.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 12/10/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Machines_Oric_Video_hpp
#define Machines_Oric_Video_hpp

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

		// Counters and limits
		int _counter, _frame_counter;
		int _v_sync_start_position, _v_sync_end_position, _counter_period;

		// Output target
		uint8_t *_pixel_target;

		// Registers
		uint8_t _ink, _paper;

		int _character_set_base_address;
		inline void set_character_set_base_address();

		bool _is_graphics_mode;
		bool _next_frame_is_sixty_hertz;
		bool _use_alternative_character_set;
		bool _use_double_height_characters;
		bool _blink_text;

		uint8_t _phase;
};

}

#endif /* Video_hpp */
