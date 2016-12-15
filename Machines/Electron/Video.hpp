//
//  Video.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/12/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Machines_Electron_Video_hpp
#define Machines_Electron_Video_hpp

#include "../../Outputs/CRT/CRT.hpp"
#include "Interrupts.hpp"

namespace Electron {

class VideoOutput {
	public:
		VideoOutput(uint8_t *memory);
		std::shared_ptr<Outputs::CRT::CRT> get_crt();
		void run_for_cycles(int number_of_cycles);

		struct Interrupt {
			Electron::Interrupt interrupt;
			int cycles;
		};
		Interrupt get_next_interrupt();

		void set_register(int address, uint8_t value);

		unsigned int get_cycles_until_next_ram_availability(int from_time);

	private:
		inline void start_pixel_line();
		inline void end_pixel_line();
		inline void output_pixels(unsigned int number_of_cycles);

		int output_position_, unused_cycles_;

		uint8_t palette_[16];
		uint8_t screen_mode_;
		uint16_t screen_mode_base_address_;
		uint16_t start_screen_address_;

		uint8_t *ram_;
		struct {
			uint16_t forty1bpp[256];
			uint8_t forty2bpp[256];
			uint32_t eighty1bpp[256];
			uint16_t eighty2bpp[256];
			uint8_t eighty4bpp[256];
		} palette_tables_;

		// Display generation.
		uint16_t start_line_address_, current_screen_address_;
		int current_pixel_line_, current_pixel_column_, current_character_row_;
		uint8_t last_pixel_byte_;
		bool is_blank_line_;

		// CRT output
		uint8_t *current_output_target_, *initial_output_target_;
		unsigned int current_output_divider_;

		std::shared_ptr<Outputs::CRT::CRT> crt_;

		struct DrawAction {
			enum Type {
				Sync, ColourBurst, Blank, Pixels
			} type;
			int length;
			DrawAction(Type type, int length) : type(type), length(length) {}
		};
		std::vector<DrawAction> screen_map_;
		void setup_screen_map();
		void emplace_blank_line();
		void emplace_pixel_line();
		size_t screen_map_pointer_;
		int cycles_into_draw_action_;
};

}

#endif /* Video_hpp */
