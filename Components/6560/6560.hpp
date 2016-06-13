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

class MOS6560Speaker: public ::Outputs::Filter<MOS6560Speaker> {
	public:
		MOS6560Speaker();

		void set_volume(uint8_t volume);
		void set_control(int channel, uint8_t volume);

		void get_samples(unsigned int number_of_samples, int16_t *target);
		void skip_samples(unsigned int number_of_samples);
};

class MOS6560 {
	public:
		MOS6560();
		Outputs::CRT::CRT *get_crt() { return _crt.get(); }
		Outputs::Speaker *get_speaker() { return &_speaker; }

		uint16_t get_address();
		void set_graphics_value(uint8_t value, uint8_t colour_value);

		void set_register(int address, uint8_t value);
		uint8_t get_register(int address);

	private:
		std::unique_ptr<Outputs::CRT::CRT> _crt;
		MOS6560Speaker _speaker;

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
