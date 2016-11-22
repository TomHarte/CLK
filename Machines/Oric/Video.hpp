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
		uint8_t *ram_;
		std::shared_ptr<Outputs::CRT::CRT> crt_;

		// Counters and limits
		int counter_, frame_counter_;
		int v_sync_start_position_, v_sync_end_position_, counter_period_;

		// Output target
		uint8_t *pixel_target_;

		// Registers
		uint8_t ink_, paper_;

		int character_set_base_address_;
		inline void set_character_set_base_address();

		bool is_graphics_mode_;
		bool next_frame_is_sixty_hertz_;
		bool use_alternative_character_set_;
		bool use_double_height_characters_;
		bool blink_text_;

		uint8_t phase_;
};

}

#endif /* Video_hpp */
