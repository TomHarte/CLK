//
//  TIA.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 28/01/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef TIA_hpp
#define TIA_hpp

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>

#include "../../../Outputs/CRT/CRT.hpp"
#include "../../../ClockReceiver/ClockReceiver.hpp"

namespace Atari2600 {

class TIA {
	public:
		TIA();

		enum class OutputMode {
			NTSC, PAL
		};

		/*!
			Advances the TIA by @c cycles. Any queued setters take effect in the first cycle performed.
		*/
		void run_for(const Cycles cycles);
		void set_output_mode(OutputMode output_mode);

		void set_sync(bool sync);
		void set_blank(bool blank);
		void reset_horizontal_counter();						// Reset is delayed by four cycles.

		/*!
			@returns the number of cycles between (current TIA time) + from_offset to the current or
			next horizontal blanking period. Returns numbers in the range [0, 227].
		*/
		int get_cycles_until_horizontal_blank(const Cycles from_offset);

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
		void set_missile_position_to_player(int missile, bool lock);
		void set_missile_motion(int missile, uint8_t motion);

		void set_ball_enable(bool enabled);
		void set_ball_delay(bool delay);
		void set_ball_position();
		void set_ball_motion(uint8_t motion);

		void move();
		void clear_motion();

		uint8_t get_collision_flags(int offset);
		void clear_collision_flags();

		void set_crt_delegate(Outputs::CRT::Delegate *);
		void set_scan_target(Outputs::Display::ScanTarget *);
		Outputs::Display::ScanStatus get_scaled_scan_status() const;

	private:
		Outputs::CRT::CRT crt_;

		// the master counter; counts from 0 to 228 with all visible pixels being in the final 160
		int horizontal_counter_ = 0;

		// contains flags to indicate whether sync or blank are currently active
		int output_mode_ = 0;

		// keeps track of the target pixel buffer for this line and when it was acquired, and a corresponding collision buffer
		alignas(alignof(uint32_t)) uint8_t collision_buffer_[160];
		enum class CollisionType : uint8_t {
			Playfield	= (1 << 0),
			Ball		= (1 << 1),
			Player0		= (1 << 2),
			Player1		= (1 << 3),
			Missile0	= (1 << 4),
			Missile1	= (1 << 5)
		};

		int collision_flags_ = 0;
		int collision_flags_by_buffer_vaules_[64];

		// colour mapping tables
		enum class ColourMode {
			Standard = 0,
			ScoreLeft,
			ScoreRight,
			OnTop
		};
		uint8_t colour_mask_by_mode_collision_flags_[4][64];	// maps from [ColourMode][CollisionMark] to colour_palette_ entry

		enum class ColourIndex {
			Background = 0,
			PlayfieldBall,
			PlayerMissile0,
			PlayerMissile1
		};
		struct Colour {
			uint16_t luminance_phase;
			uint8_t original;
		};
		std::array<Colour, 4> colour_palette_;
		void set_colour_palette_entry(size_t index, uint8_t colour);
		OutputMode tv_standard_;

		// playfield state
		int background_half_mask_ = 0;
		enum class PlayfieldPriority {
			Standard,
			Score,
			OnTop
		} playfield_priority_;
		uint32_t background_[2] = {0, 0};
				// contains two 20-bit bitfields representing the background state;
				// at index 0 is the left-hand side of the playfield with bit 0 being
				// the first bit to display, bit 1 the second, etc. Index 1 contains
				// a mirror image of index 0. If the playfield is being displayed in
				// mirroring mode, background_[0] will be output on the left and
				// background_[1] on the right; otherwise background_[0] will be
				// output twice.

		// objects
		template<class T> struct Object {
			// the two programmer-set values
			int position = 0;
			int motion = 0;

			// motion_step_ is the current motion counter value; motion_time_ is the next time it will fire
			int motion_step = 0;
			int motion_time = 0;

			// indicates whether this object is currently undergoing motion
			bool is_moving = false;
		};

		// player state
		struct Player: public Object<Player> {
			int adder = 4;
			int copy_flags = 0;				// a bit field, corresponding to the first few values of NUSIZ
			uint8_t graphic[2] = {0, 0};	// the player graphic; 1 = new, 0 = current
			int reverse_mask = false;		// 7 for a reflected player, 0 for normal
			int graphic_index = 0;

			int pixel_position = 32, pixel_counter = 0;
			int latched_pixel4_time = -1;
			const bool enqueues = true;

			inline void skip_pixels(const int count, int from_horizontal_counter) {
				int old_pixel_counter = pixel_counter;
				pixel_position = std::min(32, pixel_position + count * adder);
				pixel_counter += count;
				if(!copy_index_ && old_pixel_counter < 4 && pixel_counter >= 4) {
					latched_pixel4_time = from_horizontal_counter + 4 - old_pixel_counter;
				}
			}

			inline void reset_pixels(int copy) {
				pixel_position = pixel_counter = 0;
				copy_index_ = copy;
			}

			inline void output_pixels(uint8_t *const target, const int count, const uint8_t collision_identity, int from_horizontal_counter) {
				output_pixels(target, count, collision_identity, pixel_position, adder, reverse_mask);
				skip_pixels(count, from_horizontal_counter);
			}

			void dequeue_pixels(uint8_t *const target, const uint8_t collision_identity, const int time_now) {
				while(queue_read_pointer_ != queue_write_pointer_) {
					uint8_t *const start_ptr = &target[queue_[queue_read_pointer_].start];
					if(queue_[queue_read_pointer_].end > time_now) {
						const int length = time_now - queue_[queue_read_pointer_].start;
						output_pixels(start_ptr, length, collision_identity, queue_[queue_read_pointer_].pixel_position, queue_[queue_read_pointer_].adder, queue_[queue_read_pointer_].reverse_mask);
						queue_[queue_read_pointer_].pixel_position += length * queue_[queue_read_pointer_].adder;
						queue_[queue_read_pointer_].start = time_now;
						return;
					} else {
						output_pixels(start_ptr, queue_[queue_read_pointer_].end - queue_[queue_read_pointer_].start, collision_identity, queue_[queue_read_pointer_].pixel_position, queue_[queue_read_pointer_].adder, queue_[queue_read_pointer_].reverse_mask);
					}
					queue_read_pointer_ = (queue_read_pointer_ + 1)&3;
				}
			}

			void enqueue_pixels(const int start, const int end, int from_horizontal_counter) {
				queue_[queue_write_pointer_].start = start;
				queue_[queue_write_pointer_].end = end;
				queue_[queue_write_pointer_].pixel_position = pixel_position;
				queue_[queue_write_pointer_].adder = adder;
				queue_[queue_write_pointer_].reverse_mask = reverse_mask;
				queue_write_pointer_ = (queue_write_pointer_ + 1)&3;
				skip_pixels(end - start, from_horizontal_counter);
			}

			private:
				int copy_index_ = 0;
				struct QueuedPixels {
					int start = 0, end = 0;
					int pixel_position = 0;
					int adder = 0;
					int reverse_mask = false;
				} queue_[4];
				int queue_read_pointer_ = 0, queue_write_pointer_ = 0;

				inline void output_pixels(uint8_t *const target, const int count, const uint8_t collision_identity, int output_pixel_position, int output_adder, int output_reverse_mask) {
					if(output_pixel_position == 32 || !graphic[graphic_index]) return;
					int output_cursor = 0;
					while(output_pixel_position < 32 && output_cursor < count) {
						int shift = (output_pixel_position >> 2) ^ output_reverse_mask;
						target[output_cursor] |= ((graphic[graphic_index] >> shift)&1) * collision_identity;
						output_cursor++;
						output_pixel_position += output_adder;
					}
				}

		} player_[2];

		// common actor for things that appear as a horizontal run of pixels
		struct HorizontalRun: public Object<HorizontalRun> {
			int pixel_position = 0;
			int size = 1;
			const bool enqueues = false;

			inline void skip_pixels(const int count, int) {
				pixel_position = std::max(0, pixel_position - count);
			}

			inline void reset_pixels(int) {
				pixel_position = size;
			}

			inline void output_pixels(uint8_t *const target, const int count, const uint8_t collision_identity, [[maybe_unused]] int from_horizontal_counter) {
				int output_cursor = 0;
				while(pixel_position && output_cursor < count)
				{
					target[output_cursor] |= collision_identity;
					output_cursor++;
					pixel_position--;
				}
			}

			void dequeue_pixels([[maybe_unused]] uint8_t *const target, [[maybe_unused]] uint8_t collision_identity, [[maybe_unused]] int time_now) {}
			void enqueue_pixels([[maybe_unused]] int start, [[maybe_unused]] int end, [[maybe_unused]] int from_horizontal_counter) {}
		};

		// missile state
		struct Missile: public HorizontalRun {
			bool enabled = false;
			bool locked_to_player = false;
			int copy_flags = 0;

			inline void output_pixels(uint8_t *const target, const int count, const uint8_t collision_identity, int from_horizontal_counter) {
				if(!pixel_position) return;
				if(enabled && !locked_to_player) {
					HorizontalRun::output_pixels(target, count, collision_identity, from_horizontal_counter);
				} else {
					skip_pixels(count, from_horizontal_counter);
				}
			}
		} missile_[2];

		// ball state
		struct Ball: public HorizontalRun {
			bool enabled[2] = {false, false};
			int enabled_index = 0;
			const int copy_flags = 0;

			inline void output_pixels(uint8_t *const target, const int count, const uint8_t collision_identity, int from_horizontal_counter) {
				if(!pixel_position) return;
				if(enabled[enabled_index]) {
					HorizontalRun::output_pixels(target, count, collision_identity, from_horizontal_counter);
				} else {
					skip_pixels(count, from_horizontal_counter);
				}
			}
		} ball_;

		// motion
		bool horizontal_blank_extend_ = false;
		template<class T> void perform_border_motion(T &object, int start, int end);
		template<class T> void perform_motion_step(T &object);

		// drawing methods and state
		void draw_missile(Missile &, Player &, const uint8_t collision_identity, int start, int end);
		template<class T> void draw_object(T &, const uint8_t collision_identity, int start, int end);
		template<class T> void draw_object_visible(T &, const uint8_t collision_identity, int start, int end, int time_now);
		inline void draw_playfield(int start, int end);

		inline void output_for_cycles(int number_of_cycles);
		inline void output_line();

		int pixels_start_location_ = 0;
		uint16_t *pixel_target_ = nullptr;
		inline void output_pixels(int start, int end);
};

}

#endif /* TIA_hpp */
