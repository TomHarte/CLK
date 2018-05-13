//
//  9918Base.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/12/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef TMS9918Base_hpp
#define TMS9918Base_hpp

#include "../../../Outputs/CRT/CRT.hpp"
#include "../../../ClockReceiver/ClockReceiver.hpp"

#include <cstdint>
#include <memory>

namespace TI {

class TMS9918Base {
	protected:
		TMS9918Base();

		std::unique_ptr<Outputs::CRT::CRT> crt_;

		uint8_t ram_[16384];

		uint16_t ram_pointer_ = 0;
		uint8_t read_ahead_buffer_ = 0;
		enum class MemoryAccess {
			Read, Write, None
		} queued_access_ = MemoryAccess::None;

		uint8_t status_ = 0;

		bool write_phase_ = false;
		uint8_t low_write_ = 0;

		// The various register flags.
		int next_screen_mode_ = 0, screen_mode_ = 0;
		bool next_blank_screen_ = true, blank_screen_ = true;
		bool sprites_16x16_ = false;
		bool sprites_magnified_ = false;
		bool generate_interrupts_ = false;
		int sprite_height_ = 8;
		uint16_t pattern_name_address_ = 0;
		uint16_t colour_table_address_ = 0;
		uint16_t pattern_generator_table_address_ = 0;
		uint16_t sprite_attribute_table_address_ = 0;
		uint16_t sprite_generator_table_address_ = 0;

		uint8_t text_colour_ = 0;
		uint8_t background_colour_ = 0;

		HalfCycles half_cycles_into_frame_;
		int column_ = 0, row_ = 0, output_column_ = 0;
		int cycles_error_ = 0;
		uint32_t *pixel_target_ = nullptr, *pixel_base_ = nullptr;

		void output_border(int cycles);

		// Vertical timing details.
		int frame_lines_ = 262;
		int first_vsync_line_ = 227;

		// Horizontal selections.
		enum class LineMode {
			Text = 0,
			Character = 1,
			Refresh = 2
		} line_mode_ = LineMode::Text;
		int first_pixel_column_, first_right_border_column_;

		uint8_t pattern_names_[40];
		uint8_t pattern_buffer_[40];
		uint8_t colour_buffer_[40];

		struct SpriteSet {
			struct ActiveSprite {
				int index = 0;
				int row = 0;

				uint8_t info[4];
				uint8_t image[2];

				int shift_position = 0;
			} active_sprites[4];
			int active_sprite_slot = 0;
		} sprite_sets_[2];
		int active_sprite_set_ = 0;
		bool sprites_stopped_ = false;

		int access_pointer_ = 0;

		inline void test_sprite(int sprite_number, int screen_row);
		inline void get_sprite_contents(int start, int cycles, int screen_row);
};

}

#endif /* TMS9918Base_hpp */
