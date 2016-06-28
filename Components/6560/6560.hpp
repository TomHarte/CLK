//
//  6560.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/06/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef _560_hpp
#define _560_hpp

#include "../../Outputs/CRT/CRT.hpp"
#include "../../Outputs/Speaker.hpp"

namespace MOS {

/*!
	The 6560 Video Interface Chip ('VIC') is a video and audio output chip; it therefore vends both a @c CRT and a @c Speaker.

	To run the VIC for a cycle, the caller should call @c get_address, make the requested bus access
	and call @c set_graphics_value with the result.

	@c set_register and @c get_register provide register access.
*/
class MOS6560 {
	public:
		MOS6560();
		Outputs::CRT::CRT *get_crt() { return _crt.get(); }
		Outputs::Speaker *get_speaker() { return &_speaker; }

		enum OutputMode {
			PAL, NTSC
		};
		/*!
			Sets the output mode to either PAL or NTSC.
		*/
		void set_output_mode(OutputMode output_mode);

		/*!
			Impliedly runs the 6560 for a single cycle, returning the next address that it puts on the bus.
		*/
		uint16_t get_address();

		/*!
			An owning machine should determine the state of the data bus as a result of the access implied
			by @c get_address and supply it to set_graphics_value.
		*/
		void set_graphics_value(uint8_t value, uint8_t colour_value);

		/*!
			Causes the 6560 to flush as much pending CRT and speaker communications as possible.
		*/
		inline void synchronise() { update_audio(); }

		/*!
			Writes to a 6560 register.
		*/
		void set_register(int address, uint8_t value);

		/*
			Reads from a 6560 register.
		*/
		uint8_t get_register(int address);

	private:
		std::unique_ptr<Outputs::CRT::CRT> _crt;

		// audio state
		class Speaker: public ::Outputs::Filter<Speaker> {
			public:
				Speaker();

				void set_volume(uint8_t volume);
				void set_control(int channel, uint8_t value);

				void get_samples(unsigned int number_of_samples, int16_t *target);
				void skip_samples(unsigned int number_of_samples);

			private:
				unsigned int _counters[4];
				unsigned int _shift_registers[4];
				uint8_t _control_registers[4];
				uint8_t _volume;
		} _speaker;
		unsigned int _cycles_since_speaker_update;
		void update_audio();

		// register state
		struct {
			bool interlaced, tall_characters;
			uint8_t first_column_location, first_row_location;
			uint8_t number_of_columns, number_of_rows;
			uint16_t character_cell_start_address, video_matrix_start_address;
			uint8_t backgroundColour, borderColour, auxiliary_colour;
			bool invertedCells;

			uint8_t direct_values[16];
		} _registers;

		// output state
		enum State {
			Sync, ColourBurst, Border, Pixels
		} _this_state, _output_state;
		unsigned int _cycles_in_state;

		// counters that cover an entire field
		int _horizontal_counter, _vertical_counter, _full_frame_counter;

		// latches dictating start and length of drawing
		bool _vertical_drawing_latch, _horizontal_drawing_latch;
		int _rows_this_field, _columns_this_line;

		// current drawing position counter
		int _pixel_line_cycle, _column_counter;
		int _current_row;
		uint16_t _current_character_row;
		uint16_t _video_matrix_address_counter, _base_video_matrix_address_counter;

		// data latched from the bus
		uint8_t _character_code, _character_colour, _character_value;

		bool _is_odd_frame;

		// lookup table from 6560 colour index to appropriate PAL/NTSC value
		uint8_t _colours[16];

		uint8_t *pixel_pointer;
		void output_border(unsigned int number_of_cycles);

		struct {
			int cycles_per_line;
			int line_counter_increment_offset;
			int lines_per_progressive_field;
			bool supports_interlacing;
		} _timing;
};

}

#endif /* _560_hpp */
