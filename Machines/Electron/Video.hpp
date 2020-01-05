//
//  Video.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/12/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#ifndef Machines_Electron_Video_hpp
#define Machines_Electron_Video_hpp

#include "../../Outputs/CRT/CRT.hpp"
#include "../../ClockReceiver/ClockReceiver.hpp"
#include "Interrupts.hpp"

#include <vector>

namespace Electron {

/*!
	Implements the Electron's video subsystem plus appropriate signalling.

	The Electron has an interlaced fully-bitmapped display with six different output modes,
	running either at 40 or 80 columns. Memory is shared between video and CPU; when the video
	is accessing it the CPU may not.
*/
class VideoOutput {
	public:
		/*!
			Instantiates a VideoOutput that will read its pixels from @c memory.

			The pointer supplied should be to address 0 in the unexpanded Electron's memory map.
		*/
		VideoOutput(uint8_t *memory);

		/// Produces the next @c cycles of video output.
		void run_for(const Cycles cycles);

		/// Sets the destination for output.
		void set_scan_target(Outputs::Display::ScanTarget *scan_target);

		/// Sets the type of output.
		void set_display_type(Outputs::Display::DisplayType);

		/*!
			Writes @c value to the register at @c address. May mutate the results of @c get_next_interrupt,
			@c get_cycles_until_next_ram_availability and @c get_memory_access_range.
		*/
		void write(int address, uint8_t value);

		/*!
			Describes an interrupt the video hardware will generate by its identity and scheduling time.
		*/
		struct Interrupt {
			/// The interrupt that will be signalled.
			Electron::Interrupt interrupt;
			/// The number of cycles until it is signalled.
			int cycles;
		};
		/*!
			@returns the next interrupt that should be generated as a result of the video hardware.
			The time until signalling returned is the number of cycles after the final one triggered
			by the most recent call to @c run_for.

			This result may be mutated by calls to @c write.
		*/
		Interrupt get_next_interrupt();

		/*!
			@returns the number of cycles after (final cycle of last run_for batch + @c from_time)
			before the video circuits will allow the CPU to access RAM.
		*/
		unsigned int get_cycles_until_next_ram_availability(int from_time);

		struct Range {
			uint16_t low_address, high_address;
		};
		/*!
			@returns the range of addresses that the video might read from.
		*/
		Range get_memory_access_range();

	private:
		inline void start_pixel_line();
		inline void end_pixel_line();
		inline void output_pixels(int number_of_cycles);
		inline void setup_base_address();

		int output_position_ = 0;
		int unused_cycles_ = 0;

		uint8_t palette_[16];
		uint8_t screen_mode_ = 6;
		uint16_t screen_mode_base_address_ = 0;
		uint16_t start_screen_address_ = 0;

		uint8_t *ram_;
		struct {
			uint32_t forty1bpp[256];
			uint16_t forty2bpp[256];
			uint64_t eighty1bpp[256];
			uint32_t eighty2bpp[256];
			uint16_t eighty4bpp[256];
		} palette_tables_;

		// Display generation.
		uint16_t start_line_address_ = 0;
		uint16_t current_screen_address_ = 0;
		int current_pixel_line_ = -1;
		int current_pixel_column_ = 0;
		int current_character_row_ = 0;
		uint8_t last_pixel_byte_ = 0;
		bool is_blank_line_ = false;

		// CRT output
		uint8_t *current_output_target_ = nullptr;
		uint8_t *initial_output_target_ = nullptr;
		int current_output_divider_ = 1;
		Outputs::CRT::CRT crt_;

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
		std::size_t screen_map_pointer_ = 0;
		int cycles_into_draw_action_ = 0;
};

}

#endif /* Video_hpp */
