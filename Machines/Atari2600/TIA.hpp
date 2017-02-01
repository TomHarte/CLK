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

		// drawing methods
		inline void output_for_cycles(int number_of_cycles);
		inline void output_line();

		inline void draw_background(uint8_t *target, int start, int length);
		inline void draw_playfield(uint8_t *target, int start, int length);
		inline void draw_background_and_playfield(uint8_t *target, int start, int length);

		// the master counter; counts from 0 to 228 with all visible pixels being in the final 160
		int horizontal_counter_;

		// contains flags to indicate whether sync or blank are currently active
		int output_mode_;

		// keeps track of the target pixel buffer for this line and when it was acquired
		uint8_t *pixel_target_;
		int pixel_target_origin_;

		// playfield state
		uint8_t playfield_ball_colour_;
		uint8_t background_colour_;
		int background_half_mask_;
		uint32_t background_[2];	// contains two 20-bit bitfields representing the background state;
									// at index 0 is the left-hand side of the playfield with bit 0 being
									// the first bit to display, bit 1 the second, etc. Index 1 contains
									// a mirror image of index 0. If the playfield is being displayed in
									// mirroring mode, background_[0] will be output on the left and
									// background_[1] on the right; otherwise background_[0] will be
									// output twice.

		// player state
		struct Player {
			int size;			// 0 = normal, 1 = double, 2 = quad
			int copy_flags;		// a bit field, corresponding to the first few values of NUSIZ
			uint8_t graphic;	// the player graphic
			uint8_t colour;		// the player colour
			int reverse_mask;	// 7 for a reflected player, 0 for normal
			uint8_t motion;		// low four bits used
			uint8_t position;	// in the range [0, 160) to indicate offset from the left margin, i.e. phase difference
								// between the player counter and the background pixel counter.
		} player_[2];

		// missile state
		struct Missile {
			int size;		// 0 = 1 pixel, 1 = 2 pixels, etc
		} missile_[2];
};

}

#endif /* TIA_hpp */
