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
};

/*!
	The 6560 Video Interface Chip ('VIC') is a video and audio output chip; it therefore vends both a @c CRT and a @c Speaker.

	To run the VIC for a cycle, the caller should call @c get_address, make the requested bus access
	and call @c set_graphics_value with the result.

	@c set_register and @c get_register provide register access.
*/
template <class T> class MOS6560 {
	public:
		MOS6560() :
			_crt(new Outputs::CRT::CRT(65*4, 4, Outputs::CRT::NTSC60, 1)),
			_speaker(new Speaker),
			_horizontal_counter(0),
			_vertical_counter(0),
			_cycles_since_speaker_update(0),
			_is_odd_frame(false),
			_is_odd_line(false)
		{
			_crt->set_composite_sampling_function(
				"float composite_sample(usampler2D texID, vec2 coordinate, vec2 iCoordinate, float phase, float amplitude)"
				"{"
					"uint c = texture(texID, coordinate).r;"
					"float y = float(c >> 4) / 4.0;"
					"uint yC = c & 15u;"
					"float phaseOffset = 6.283185308 * float(yC) / 16.0;"

					"float chroma = cos(phase + phaseOffset);"
					"return mix(y, step(yC, 14) * chroma, amplitude);"
				"}");

			// default to NTSC
			set_output_mode(OutputMode::NTSC);
		}

		void set_clock_rate(double clock_rate)
		{
			_speaker->set_input_rate((float)(clock_rate / 4.0));
		}

		std::shared_ptr<Outputs::CRT::CRT> get_crt() { return _crt; }
		std::shared_ptr<Outputs::Speaker> get_speaker() { return _speaker; }

		enum OutputMode {
			PAL, NTSC
		};

		/*!
			Sets the output mode to either PAL or NTSC.
		*/
		void set_output_mode(OutputMode output_mode)
		{
			_output_mode = output_mode;
			uint8_t luminances[16] = {		// range is 0–4
				0, 4, 1, 3, 2, 2, 1, 3,
				2, 1, 2, 1, 2, 3, 2, 3
			};
			uint8_t pal_chrominances[16] = {	// range is 0–15; 15 is a special case meaning "no chrominance"
				15, 15, 5, 13, 2, 10, 0, 8,
				6, 7, 5, 13, 2, 10, 0, 8,
			};
			uint8_t ntsc_chrominances[16] = {
				15, 15, 2, 10, 4, 12, 6, 14,
				0, 8, 2, 10, 4, 12, 6, 14,
			};
			uint8_t *chrominances;
			Outputs::CRT::DisplayType display_type;

			switch(output_mode)
			{
				case OutputMode::PAL:
					chrominances = pal_chrominances;
					display_type = Outputs::CRT::PAL50;
					_timing.cycles_per_line = 71;
					_timing.line_counter_increment_offset = 0;
					_timing.lines_per_progressive_field = 312;
					_timing.supports_interlacing = false;
				break;

				case OutputMode::NTSC:
					chrominances = ntsc_chrominances;
					display_type = Outputs::CRT::NTSC60;
					_timing.cycles_per_line = 65;
					_timing.line_counter_increment_offset = 65 - 33;	// TODO: this is a bit of a hack; separate vertical and horizontal counting
					_timing.lines_per_progressive_field = 261;
					_timing.supports_interlacing = true;
				break;
			}

			_crt->set_new_display_type((unsigned int)(_timing.cycles_per_line*4), display_type);
//			_crt->set_visible_area(Outputs::CRT::Rect(0.1f, 0.1f, 0.8f, 0.8f));

//			switch(output_mode)
//			{
//				case OutputMode::PAL:
//					_crt->set_visible_area(_crt->get_rect_for_area(16, 237, 15*4, 55*4, 4.0f / 3.0f));
//				break;
//				case OutputMode::NTSC:
//					_crt->set_visible_area(_crt->get_rect_for_area(16, 237, 11*4, 55*4, 4.0f / 3.0f));
//				break;
//			}

			for(int c = 0; c < 16; c++)
			{
				_colours[c] = (uint8_t)((luminances[c] << 4) | chrominances[c]);
			}
		}

		/*!
			Runs for cycles. Derr.
		*/
		inline void run_for_cycles(unsigned int number_of_cycles)
		{
			// keep track of the amount of time since the speaker was updated; lazy updates are applied
			_cycles_since_speaker_update += number_of_cycles;

			while(number_of_cycles--)
			{
				// keep an old copy of the vertical count because that test is a cycle later than the actual changes
				int previous_vertical_counter = _vertical_counter;

				// keep track of internal time relative to this scanline
				_horizontal_counter++;
				_full_frame_counter++;
				if(_horizontal_counter == _timing.cycles_per_line)
				{
					if(_horizontal_drawing_latch)
					{
						_current_character_row++;
						if(
							(_current_character_row == 16) ||
							(_current_character_row == 8 && !_registers.tall_characters)
						) {
							_current_character_row = 0;
							_current_row++;
						}

						_pixel_line_cycle = -1;
						_columns_this_line = -1;
						_column_counter = -1;
					}

					_horizontal_counter = 0;
					if(_output_mode == OutputMode::PAL) _is_odd_line ^= true;
					_horizontal_drawing_latch = false;

					_vertical_counter ++;
					if(_vertical_counter == (_registers.interlaced ? (_is_odd_frame ? 262 : 263) : _timing.lines_per_progressive_field))
					{
						_vertical_counter = 0;
						_full_frame_counter = 0;

						if(_output_mode == OutputMode::NTSC) _is_odd_frame ^= true;
						_current_row = 0;
						_rows_this_field = -1;
						_vertical_drawing_latch = false;
						_base_video_matrix_address_counter = 0;
						_current_character_row = 0;
					}
				}

				// check for vertical starting events
				_vertical_drawing_latch |= _registers.first_row_location == (previous_vertical_counter >> 1);
				_horizontal_drawing_latch |= _vertical_drawing_latch && (_horizontal_counter == _registers.first_column_location);

				if(_pixel_line_cycle >= 0) _pixel_line_cycle++;
				switch(_pixel_line_cycle)
				{
					case -1:
						if(_horizontal_drawing_latch)
						{
							_pixel_line_cycle = 0;
							_video_matrix_address_counter = _base_video_matrix_address_counter;
						}
					break;
					case 1:	_columns_this_line = _registers.number_of_columns;	break;
					case 2:	if(_rows_this_field < 0) _rows_this_field = _registers.number_of_rows;	break;
					case 3: if(_current_row < _rows_this_field) _column_counter = 0;	break;
				}

				uint16_t fetch_address = 0x1c;
				if(_column_counter >= 0 && _column_counter < _columns_this_line*2)
				{
					if(_column_counter&1)
					{
						fetch_address = _registers.character_cell_start_address + (_character_code*(_registers.tall_characters ? 16 : 8)) + _current_character_row;
					}
					else
					{
						fetch_address = (uint16_t)(_registers.video_matrix_start_address + _video_matrix_address_counter);
						_video_matrix_address_counter++;
						if(
							(_current_character_row == 15) ||
							(_current_character_row == 7 && !_registers.tall_characters)
						) {
							_base_video_matrix_address_counter = _video_matrix_address_counter;
						}
					}
				}

				fetch_address &= 0x3fff;

				uint8_t pixel_data;
				uint8_t colour_data;
				static_cast<T *>(this)->perform_read(fetch_address, &pixel_data, &colour_data);

				// TODO: there should be a further two-cycle delay on pixels being output; the reverse bit should
				// divide the byte it is set for 3:1 and then continue as usual.

				// determine output state; colour burst and sync timing are currently a guess
				if(_horizontal_counter > _timing.cycles_per_line-4) _this_state = State::ColourBurst;
				else if(_horizontal_counter > _timing.cycles_per_line-7) _this_state = State::Sync;
				else
				{
					_this_state = (_column_counter >= 0 && _column_counter < _columns_this_line*2) ? State::Pixels : State::Border;
				}

				// apply vertical sync
				if(
					(_vertical_counter < 3 && (_is_odd_frame || !_registers.interlaced)) ||
					(_registers.interlaced &&
						(
							(_vertical_counter == 0 && _horizontal_counter > 32) ||
							(_vertical_counter == 1) || (_vertical_counter == 2) ||
							(_vertical_counter == 3 && _horizontal_counter <= 32)
						)
					))
					_this_state = State::Sync;

				// update the CRT
				if(_this_state != _output_state)
				{
					switch(_output_state)
					{
						case State::Sync:			_crt->output_sync(_cycles_in_state * 4);														break;
						case State::ColourBurst:	_crt->output_colour_burst(_cycles_in_state * 4, (_is_odd_frame || _is_odd_line) ? 128 : 0, 0);	break;
						case State::Border:			output_border(_cycles_in_state * 4);															break;
						case State::Pixels:			_crt->output_data(_cycles_in_state * 4, 1);														break;
					}
					_output_state = _this_state;
					_cycles_in_state = 0;

					pixel_pointer = nullptr;
					if(_output_state == State::Pixels)
					{
						pixel_pointer = _crt->allocate_write_area(260);
					}
				}
				_cycles_in_state++;

				if(_this_state == State::Pixels)
				{
					if(_column_counter&1)
					{
						_character_value = pixel_data;

						if(pixel_pointer)
						{
							uint8_t cell_colour = _colours[_character_colour & 0x7];
							if(!(_character_colour&0x8))
							{
								uint8_t colours[2];
								if(_registers.invertedCells)
								{
									colours[0] = cell_colour;
									colours[1] = _registers.backgroundColour;
								}
								else
								{
									colours[0] = _registers.backgroundColour;
									colours[1] = cell_colour;
								}
								pixel_pointer[0] = colours[(_character_value >> 7)&1];
								pixel_pointer[1] = colours[(_character_value >> 6)&1];
								pixel_pointer[2] = colours[(_character_value >> 5)&1];
								pixel_pointer[3] = colours[(_character_value >> 4)&1];
								pixel_pointer[4] = colours[(_character_value >> 3)&1];
								pixel_pointer[5] = colours[(_character_value >> 2)&1];
								pixel_pointer[6] = colours[(_character_value >> 1)&1];
								pixel_pointer[7] = colours[(_character_value >> 0)&1];
							}
							else
							{
								uint8_t colours[4] = {_registers.backgroundColour, _registers.borderColour, cell_colour, _registers.auxiliary_colour};
								pixel_pointer[0] =
								pixel_pointer[1] = colours[(_character_value >> 6)&3];
								pixel_pointer[2] =
								pixel_pointer[3] = colours[(_character_value >> 4)&3];
								pixel_pointer[4] =
								pixel_pointer[5] = colours[(_character_value >> 2)&3];
								pixel_pointer[6] =
								pixel_pointer[7] = colours[(_character_value >> 0)&3];
							}
							pixel_pointer += 8;
						}
					}
					else
					{
						_character_code = pixel_data;
						_character_colour = colour_data;
					}

					_column_counter++;
				}
			}
		}

		/*!
			Causes the 6560 to flush as much pending CRT and speaker communications as possible.
		*/
		inline void synchronise() { update_audio(); }

		/*!
			Writes to a 6560 register.
		*/
		void set_register(int address, uint8_t value)
		{
			address &= 0xf;
			_registers.direct_values[address] = value;
			switch(address)
			{
				case 0x0:
					_registers.interlaced = !!(value&0x80) && _timing.supports_interlacing;
					_registers.first_column_location = value & 0x7f;
				break;

				case 0x1:
					_registers.first_row_location = value;
				break;

				case 0x2:
					_registers.number_of_columns = value & 0x7f;
					_registers.video_matrix_start_address = (uint16_t)((_registers.video_matrix_start_address & 0x3c00) | ((value & 0x80) << 2));
				break;

				case 0x3:
					_registers.number_of_rows = (value >> 1)&0x3f;
					_registers.tall_characters = !!(value&0x01);
				break;

				case 0x5:
					_registers.character_cell_start_address = (uint16_t)((value & 0x0f) << 10);
					_registers.video_matrix_start_address = (uint16_t)((_registers.video_matrix_start_address & 0x0200) | ((value & 0xf0) << 6));
				break;

				case 0xa:
				case 0xb:
				case 0xc:
				case 0xd:
					update_audio();
					_speaker->set_control(address - 0xa, value);
				break;

				case 0xe:
					update_audio();
					_registers.auxiliary_colour = _colours[value >> 4];
					_speaker->set_volume(value & 0xf);
				break;

				case 0xf:
				{
					uint8_t new_border_colour = _colours[value & 0x07];
					if(_this_state == State::Border && new_border_colour != _registers.borderColour)
					{
						output_border(_cycles_in_state * 4);
						_cycles_in_state = 0;
					}
					_registers.invertedCells = !((value >> 3)&1);
					_registers.borderColour = new_border_colour;
					_registers.backgroundColour = _colours[value >> 4];
				}
				break;

				// TODO: the lightpen, etc

				default:
				break;
			}
		}

		/*
			Reads from a 6560 register.
		*/
		uint8_t get_register(int address)
		{
			address &= 0xf;
			int current_line = (_full_frame_counter + _timing.line_counter_increment_offset) / _timing.cycles_per_line;
			switch(address)
			{
				default: return _registers.direct_values[address];
				case 0x03: return (uint8_t)(current_line << 7) | (_registers.direct_values[3] & 0x7f);
				case 0x04: return (current_line >> 1) & 0xff;
			}
		}

	private:
		std::shared_ptr<Outputs::CRT::CRT> _crt;

		std::shared_ptr<Speaker> _speaker;
		unsigned int _cycles_since_speaker_update;
		void update_audio()
		{
			_speaker->run_for_cycles(_cycles_since_speaker_update >> 2);
			_cycles_since_speaker_update &= 3;
		}

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

		bool _is_odd_frame, _is_odd_line;

		// lookup table from 6560 colour index to appropriate PAL/NTSC value
		uint8_t _colours[16];

		uint8_t *pixel_pointer;
		void output_border(unsigned int number_of_cycles)
		{
			uint8_t *colour_pointer = _crt->allocate_write_area(1);
			if(colour_pointer) *colour_pointer = _registers.borderColour;
			_crt->output_level(number_of_cycles);
		}

		struct {
			int cycles_per_line;
			int line_counter_increment_offset;
			int lines_per_progressive_field;
			bool supports_interlacing;
		} _timing;
		OutputMode _output_mode;
};

}

#endif /* _560_hpp */
