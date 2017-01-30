//
//  TIA.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 28/01/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef TIA_hpp
#define TIA_hpp

#include <cstdint>
#include "../CRTMachine.hpp"

namespace Atari2600 {

class TIA {
	public:
		TIA();
		~TIA();

		enum class OutputMode {
			NTSC, PAL
		};

		void run_for_cycles(int number_of_cycles);
		void set_output_mode(OutputMode output_mode);

		void set_sync(bool sync);
		void set_blank(bool blank);
		void reset_horizontal_counter(); 						// Reset is delayed by four cycles.

		int get_cycles_until_horizontal_blank(unsigned int from_offset);

		void set_background_colour(uint8_t colour);

		void set_playfield(uint16_t offset, uint8_t value);
		void set_playfield_control_and_ball_size(uint8_t value);
		void set_playfield_ball_colour(uint8_t colour);

		void set_player_number_and_size(int player, uint8_t value);
		void set_player_graphic(int player, uint8_t value);
		void set_player_reflected(int player, bool reflected);
		void set_player_delay(int player, bool delay);
		void set_player_position(int player);
		void set_player_motion(int player, uint8_t motion);
		void set_player_missile_colour(int player, uint8_t colour);

		void set_missile_enable(int missile, bool enabled);
		void set_missile_position(int missile);
		void set_missile_position_to_player(int missile);
		void set_missile_motion(int missile, uint8_t motion);

		void set_ball_enable(bool enabled);
		void set_ball_delay(bool delay);
		void set_ball_position();
		void set_ball_motion(uint8_t motion);

		void move();
		void clear_motion();

		uint8_t get_collision_flags(int offset);
		void clear_collision_flags();

		virtual std::shared_ptr<Outputs::CRT::CRT> get_crt() { return crt_; }

	private:
		std::shared_ptr<Outputs::CRT::CRT> crt_;

		int horizontal_counter_;

		int output_cursor_;
		int output_mode_;
		int requested_output_mode_;

		uint8_t *pixel_target_;

		void output_for_cycles(int number_of_cycles);
		void output_line();
};

}

#endif /* TIA_hpp */
