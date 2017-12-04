//
//  9918.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/11/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef _918_hpp
#define _918_hpp

#include "../../Outputs/CRT/CRT.hpp"
#include "../../ClockReceiver/ClockReceiver.hpp"

#include <cstdint>

namespace TI {

class TMS9918 {
	public:
		enum Personality {
			TMS9918A,	// includes the 9928A; set TV standard as desired.
		};

		/*!
			Constructs an instance of the drive controller that behaves according to personality @c p.
			@param p The type of controller to emulate.
		*/
		TMS9918(Personality p);

		enum TVStandard {
			PAL, NTSC
		};
		void set_tv_standard(TVStandard standard);

		std::shared_ptr<Outputs::CRT::CRT> get_crt();

		/*!
			Runs the VCP for the number of cycles indicate; it is an implicit assumption of the code
			that the input clock rate is 3579545 Hz — the NTSC colour clock rate.
		*/
		void run_for(const HalfCycles cycles);

		void set_register(int address, uint8_t value);
		uint8_t get_register(int address);

		HalfCycles get_time_until_interrupt();
		bool get_interrupt_line();

	private:
		std::shared_ptr<Outputs::CRT::CRT> crt_;
	
		uint8_t ram_[16384];

		uint16_t ram_pointer_ = 0;
		uint8_t read_ahead_buffer_ = 0;

		uint8_t status_ = 0;

		bool write_phase_ = false;
		uint8_t low_write_ = 0;

		// The various register flags.
		int next_screen_mode_ = 0, screen_mode_ = 0;
		bool next_blank_screen_ = true, blank_screen_ = true;
		bool sprites_16x16_ = false;
		bool sprites_magnified_ = false;
		bool generate_interrupts_ = false;
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
			Text,
			Character
		} line_mode_ = LineMode::Text;
		int first_pixel_column_, first_right_border_column_;

		uint8_t pattern_buffer_[40];
		uint8_t colour_buffer_[40];
		uint8_t sprite_locations_[32];
		int active_sprites_[4];
		int access_pointer_ = 0;
		uint8_t pattern_name_ = 0;

		struct Sprite {
			uint8_t x, y;
			uint8_t colour, pattern_number;
			uint8_t pattern[2];
		} sprites_[4];
};

};

#endif /* _918_hpp */
