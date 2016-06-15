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
	The 6560 is a video and audio output chip; it therefore vends both a @c CRT and a @c Speaker.

	To run the 6560 for a cycle, the caller should call @c get_address, make the requested bus access
	and call @c set_graphics_value with the result.

	@c set_register and @c get_register provide register access.
*/
class MOS6560 {
	public:
		MOS6560();
		Outputs::CRT::CRT *get_crt() { return _crt.get(); }
		Outputs::Speaker *get_speaker() { return &_speaker; }

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

		bool _interlaced, _tall_characters;
		uint8_t _first_column_location, _first_row_location;
		uint8_t _number_of_columns, _number_of_rows;
		uint16_t _character_cell_start_address, _video_matrix_start_address;
		uint8_t _backgroundColour, _borderColour, _auxiliary_colour;
		bool _invertedCells;

		int _horizontal_counter, _vertical_counter;

		int _column_counter, _row_counter;
		uint16_t _video_matrix_address_counter, _video_matrix_line_address_counter;

		enum State {
			Sync, ColourBurst, Border, Pixels
		} _this_state, _output_state;
		unsigned int _cycles_in_state;

		uint8_t _character_code, _character_colour, _character_value;

		uint8_t *pixel_pointer;

		uint8_t _registers[16];
		uint8_t _colours[16];

		bool _is_odd_frame;

		void output_border(unsigned int number_of_cycles);

		unsigned int _cycles_since_speaker_update;
		void update_audio();
};

}

#endif /* _560_hpp */
