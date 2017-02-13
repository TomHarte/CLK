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

		// The supplied hook is for unit testing only; if instantiated with a line_end_function then it will
		// be called with the latest collision buffer upon the conclusion of each line. What's a collision
		// buffer? It's an implementation detail. If you're not writing a unit test, leave it alone.
		TIA(std::function<void(uint8_t *output_buffer)> line_end_function);

		enum class OutputMode {
			NTSC, PAL
		};

		/*!
			Advances the TIA by @c number_of_cycles cycles. Any queued setters take effect in the
			first cycle performed.
		*/
		void run_for_cycles(int number_of_cycles);
		void set_output_mode(OutputMode output_mode);

		void set_sync(bool sync);
		void set_blank(bool blank);
		void reset_horizontal_counter(); 						// Reset is delayed by four cycles.

		/*!
			@returns the number of cycles between (current TIA time) + from_offset to the current or
			next horizontal blanking period. Returns numbers in the range [0, 227].
		*/
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
		std::function<void(uint8_t *output_buffer)> line_end_function_;

		// the master counter; counts from 0 to 228 with all visible pixels being in the final 160
		int horizontal_counter_;

		// contains flags to indicate whether sync or blank are currently active
		int output_mode_;

		// keeps track of the target pixel buffer for this line and when it was acquired, and a corresponding collision buffer
//		alignas(alignof(uint32_t)) uint8_t collision_buffer_[160];
		std::vector<uint8_t> collision_buffer_;
		enum class CollisionType : uint8_t {
			Playfield	= (1 << 0),
			Ball		= (1 << 1),
			Player0		= (1 << 2),
			Player1		= (1 << 3),
			Missile0	= (1 << 4),
			Missile1	= (1 << 5)
		};

		int collision_flags_;
		int collision_flags_by_buffer_vaules_[64];

		// colour mapping tables
		enum class ColourMode {
			Standard = 0,
			ScoreLeft,
			ScoreRight,
			OnTop
		};
		uint8_t colour_mask_by_mode_collision_flags_[4][64];	// maps from [ColourMode][CollisionMark] to colour_pallete_ entry

		enum class ColourIndex {
			Background = 0,
			PlayfieldBall,
			PlayerMissile0,
			PlayerMissile1
		};
		uint8_t colour_palette_[4];

		// playfield state
		int background_half_mask_;
		enum class PlayfieldPriority {
			Standard,
			Score,
			OnTop
		} playfield_priority_;
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
			uint8_t graphic[2];	// the player graphic
			int reverse_mask;	// 7 for a reflected player, 0 for normal

			int pixel_position;
			int output_delay;
			bool graphic_delay;

			Player() : size(0), copy_flags(0), graphic{0, 0}, reverse_mask(false), pixel_position(32), output_delay(0), graphic_delay(false) {}
		} player_[2];

		// missile state
		struct Missile {
			int size;		// 0 = 1 pixel, 1 = 2 pixels, etc
		} missile_[2];

		// movement
		bool horizontal_blank_extend_;
		int horizontal_move_start_time_;
		int motion_[5];
		int position_[5];
		bool is_moving_[5];
		enum class MotionIndex : uint8_t {
			Ball,
			Player0,
			Player1,
			Missile0,
			Missile1
		};
		inline int perform_border_motion(int identity, int start, int end, int &movement_time);
		inline void perform_motion_step(int identity, int movement_time);

		// drawing methods and state
		inline void output_for_cycles(int number_of_cycles);
		inline void output_line();

		inline void draw_playfield(int start, int end);
		inline void draw_player(Player &player, CollisionType collision_identity, const int position_identity, int start, int end);

		inline void update_motion(int start, int end);

		int pixels_start_location_;
		uint8_t *pixel_target_;
		inline void output_pixels(int start, int end);
};

}

#endif /* TIA_hpp */
