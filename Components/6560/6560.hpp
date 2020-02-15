//
//  6560.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/06/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#ifndef _560_hpp
#define _560_hpp

#include "../../ClockReceiver/ClockReceiver.hpp"
#include "../../Concurrency/AsyncTaskQueue.hpp"
#include "../../Outputs/CRT/CRT.hpp"
#include "../../Outputs/Speaker/Implementation/LowpassSpeaker.hpp"
#include "../../Outputs/Speaker/Implementation/SampleSource.hpp"

namespace MOS {
namespace MOS6560 {

// audio state
class AudioGenerator: public ::Outputs::Speaker::SampleSource {
	public:
		AudioGenerator(Concurrency::DeferringAsyncTaskQueue &audio_queue);

		void set_volume(uint8_t volume);
		void set_control(int channel, uint8_t value);

		// For ::SampleSource.
		void get_samples(std::size_t number_of_samples, int16_t *target);
		void skip_samples(std::size_t number_of_samples);
		void set_sample_volume_range(std::int16_t range);

	private:
		Concurrency::DeferringAsyncTaskQueue &audio_queue_;

		unsigned int counters_[4] = {2, 1, 0, 0};	// create a slight phase offset for the three channels
		unsigned int shift_registers_[4] = {0, 0, 0, 0};
		uint8_t control_registers_[4] = {0, 0, 0, 0};
		int16_t volume_ = 0;
		int16_t range_multiplier_ = 1;
};

struct BusHandler {
	void perform_read(uint16_t address, uint8_t *pixel_data, uint8_t *colour_data) {
		*pixel_data = 0xff;
		*colour_data = 0xff;
	}
};

enum class OutputMode {
	PAL, NTSC
};

/*!
	The 6560 Video Interface Chip ('VIC') is a video and audio output chip; it therefore vends both a @c CRT and a @c Speaker.

	To run the VIC for a cycle, the caller should call @c get_address, make the requested bus access
	and call @c set_graphics_value with the result.

	@c write and @c read provide register access.
*/
template <class BusHandler> class MOS6560 {
	public:
		MOS6560(BusHandler &bus_handler) :
				bus_handler_(bus_handler),
				crt_(65*4, 1, Outputs::Display::Type::NTSC60, Outputs::Display::InputDataType::Luminance8Phase8),
				audio_generator_(audio_queue_),
				speaker_(audio_generator_)
		{
			// default to s-video output
			crt_.set_display_type(Outputs::Display::DisplayType::SVideo);

			// default to NTSC
			set_output_mode(OutputMode::NTSC);
		}

		~MOS6560() {
			audio_queue_.flush();
		}

		void set_clock_rate(double clock_rate) {
			speaker_.set_input_rate(static_cast<float>(clock_rate / 4.0));
		}

		void set_scan_target(Outputs::Display::ScanTarget *scan_target)		{ crt_.set_scan_target(scan_target); 			}
		Outputs::Display::ScanStatus get_scaled_scan_status() const			{ return crt_.get_scaled_scan_status() / 4.0f;	}
		void set_display_type(Outputs::Display::DisplayType display_type)	{ crt_.set_display_type(display_type); 			}
		Outputs::Speaker::Speaker *get_speaker() { return &speaker_; }

		void set_high_frequency_cutoff(float cutoff) {
			speaker_.set_high_frequency_cutoff(cutoff);
		}

		/*!
			Sets the output mode to either PAL or NTSC.
		*/
		void set_output_mode(OutputMode output_mode) {
			output_mode_ = output_mode;

			// Luminances are encoded trivially: on a 0-255 scale.
			const uint8_t luminances[16] = {
				0,		255,	64,		192,
				128,	128,	64,		192,
				128,	192,	128,	255,
				192,	192,	128,	255
			};

			// Chrominances are encoded such that 0-128 is a complete revolution of phase;
			// anything above 191 disables the colour subcarrier. Phase is relative to the
			// colour burst, so 0 is green (NTSC) or blue/violet (PAL).
			const uint8_t pal_chrominances[16] = {
				255,	255,	90,		20,
				96,		42,		8,		72,
				84,		90,		90,		20,
				96,		42,		8,		72,
			};
			const uint8_t ntsc_chrominances[16] = {
				255,	255,	121,	57,
				103,	42,		80,		16,
				0,		9,		121,	57,
				103,	42,		80,		16,
			};
			const uint8_t *chrominances;
			Outputs::Display::Type display_type;

			switch(output_mode) {
				default:
					chrominances = pal_chrominances;
					display_type = Outputs::Display::Type::PAL50;
					timing_.cycles_per_line = 71;
					timing_.line_counter_increment_offset = 4;
					timing_.final_line_increment_position = timing_.cycles_per_line - timing_.line_counter_increment_offset;
					timing_.lines_per_progressive_field = 312;
					timing_.supports_interlacing = false;
				break;

				case OutputMode::NTSC:
					chrominances = ntsc_chrominances;
					display_type = Outputs::Display::Type::NTSC60;
					timing_.cycles_per_line = 65;
					timing_.line_counter_increment_offset = 40;
					timing_.final_line_increment_position = 58;
					timing_.lines_per_progressive_field = 261;
					timing_.supports_interlacing = true;
				break;
			}

			crt_.set_new_display_type(timing_.cycles_per_line*4, display_type);

			switch(output_mode) {
				case OutputMode::PAL:
					crt_.set_visible_area(Outputs::Display::Rect(0.1f, 0.07f, 0.9f, 0.9f));
				break;
				case OutputMode::NTSC:
					crt_.set_visible_area(Outputs::Display::Rect(0.05f, 0.05f, 0.9f, 0.9f));
				break;
			}

			for(int c = 0; c < 16; c++) {
				uint8_t *colour = reinterpret_cast<uint8_t *>(&colours_[c]);
				colour[0] = luminances[c];
				colour[1] = chrominances[c];
			}
		}

		/*!
			Runs for cycles. Derr.
		*/
		inline void run_for(const Cycles cycles) {
			// keep track of the amount of time since the speaker was updated; lazy updates are applied
			cycles_since_speaker_update_ += cycles;

			auto number_of_cycles = cycles.as_integral();
			while(number_of_cycles--) {
				// keep an old copy of the vertical count because that test is a cycle later than the actual changes
				int previous_vertical_counter = vertical_counter_;

				// keep track of internal time relative to this scanline
				horizontal_counter_++;
				if(horizontal_counter_ == timing_.cycles_per_line) {
					if(horizontal_drawing_latch_) {
						current_character_row_++;
						if(
							(current_character_row_ == 16) ||
							(current_character_row_ == 8 && !registers_.tall_characters)
						) {
							current_character_row_ = 0;
							current_row_++;
						}

						pixel_line_cycle_ = -1;
						columns_this_line_ = -1;
						column_counter_ = -1;
					}

					horizontal_counter_ = 0;
					if(output_mode_ == OutputMode::PAL) is_odd_line_ ^= true;
					horizontal_drawing_latch_ = false;

					vertical_counter_ ++;
					if(vertical_counter_ == lines_this_field()) {
						vertical_counter_ = 0;

						if(output_mode_ == OutputMode::NTSC) is_odd_frame_ ^= true;
						current_row_ = 0;
						rows_this_field_ = -1;
						vertical_drawing_latch_ = false;
						base_video_matrix_address_counter_ = 0;
						current_character_row_ = 0;
					}
				}

				// check for vertical starting events
				vertical_drawing_latch_ |= registers_.first_row_location == (previous_vertical_counter >> 1);
				horizontal_drawing_latch_ |= vertical_drawing_latch_ && (horizontal_counter_ == registers_.first_column_location);

				if(pixel_line_cycle_ >= 0) pixel_line_cycle_++;
				switch(pixel_line_cycle_) {
					case -1:
						if(horizontal_drawing_latch_) {
							pixel_line_cycle_ = 0;
							video_matrix_address_counter_ = base_video_matrix_address_counter_;
						}
					break;
					case 1:	columns_this_line_ = registers_.number_of_columns;	break;
					case 2:	if(rows_this_field_ < 0) rows_this_field_ = registers_.number_of_rows;	break;
					case 3: if(current_row_ < rows_this_field_) column_counter_ = 0;	break;
				}

				uint16_t fetch_address = 0x1c;
				if(column_counter_ >= 0 && column_counter_ < columns_this_line_*2) {
					if(column_counter_&1) {
						fetch_address = registers_.character_cell_start_address + (character_code_*(registers_.tall_characters ? 16 : 8)) + current_character_row_;
					} else {
						fetch_address = static_cast<uint16_t>(registers_.video_matrix_start_address + video_matrix_address_counter_);
						video_matrix_address_counter_++;
						if(
							(current_character_row_ == 15) ||
							(current_character_row_ == 7 && !registers_.tall_characters)
						) {
							base_video_matrix_address_counter_ = video_matrix_address_counter_;
						}
					}
				}

				fetch_address &= 0x3fff;

				uint8_t pixel_data;
				uint8_t colour_data;
				bus_handler_.perform_read(fetch_address, &pixel_data, &colour_data);

				// TODO: there should be a further two-cycle delay on pixels being output; the reverse bit should
				// divide the byte it is set for 3:1 and then continue as usual.

				// determine output state; colour burst and sync timing are currently a guess
				if(horizontal_counter_ > timing_.cycles_per_line-4) this_state_ = State::ColourBurst;
				else if(horizontal_counter_ > timing_.cycles_per_line-7) this_state_ = State::Sync;
				else {
					this_state_ = (column_counter_ >= 0 && column_counter_ < columns_this_line_*2) ? State::Pixels : State::Border;
				}

				// apply vertical sync
				if(
					(vertical_counter_ < 3 && is_odd_frame()) ||
					(registers_.interlaced &&
						(
							(vertical_counter_ == 0 && horizontal_counter_ > 32) ||
							(vertical_counter_ == 1) || (vertical_counter_ == 2) ||
							(vertical_counter_ == 3 && horizontal_counter_ <= 32)
						)
					))
					this_state_ = State::Sync;

				// update the CRT
				if(this_state_ != output_state_) {
					switch(output_state_) {
						case State::Sync:			crt_.output_sync(cycles_in_state_ * 4);														break;
						case State::ColourBurst:	crt_.output_colour_burst(cycles_in_state_ * 4, (is_odd_frame_ || is_odd_line_) ? 128 : 0);	break;
						case State::Border:			output_border(cycles_in_state_ * 4);														break;
						case State::Pixels:			crt_.output_data(cycles_in_state_ * 4);														break;
					}
					output_state_ = this_state_;
					cycles_in_state_ = 0;

					pixel_pointer = nullptr;
					if(output_state_ == State::Pixels) {
						pixel_pointer = reinterpret_cast<uint16_t *>(crt_.begin_data(260));
					}
				}
				cycles_in_state_++;

				if(this_state_ == State::Pixels) {
					// TODO: palette changes can happen within half-characters; the below needs to be divided.
					// Also: a perfect opportunity to rearrange this inner loop for no longer needing to be
					// two parts with a cooperative owner?
					if(column_counter_&1) {
						character_value_ = pixel_data;

						if(pixel_pointer) {
							uint16_t cell_colour = colours_[character_colour_ & 0x7];
							if(!(character_colour_&0x8)) {
								uint16_t colours[2];
								if(registers_.invertedCells) {
									colours[0] = cell_colour;
									colours[1] = registers_.backgroundColour;
								} else {
									colours[0] = registers_.backgroundColour;
									colours[1] = cell_colour;
								}
								pixel_pointer[0] = colours[(character_value_ >> 7)&1];
								pixel_pointer[1] = colours[(character_value_ >> 6)&1];
								pixel_pointer[2] = colours[(character_value_ >> 5)&1];
								pixel_pointer[3] = colours[(character_value_ >> 4)&1];
								pixel_pointer[4] = colours[(character_value_ >> 3)&1];
								pixel_pointer[5] = colours[(character_value_ >> 2)&1];
								pixel_pointer[6] = colours[(character_value_ >> 1)&1];
								pixel_pointer[7] = colours[(character_value_ >> 0)&1];
							} else {
								uint16_t colours[4] = {registers_.backgroundColour, registers_.borderColour, cell_colour, registers_.auxiliary_colour};
								pixel_pointer[0] =
								pixel_pointer[1] = colours[(character_value_ >> 6)&3];
								pixel_pointer[2] =
								pixel_pointer[3] = colours[(character_value_ >> 4)&3];
								pixel_pointer[4] =
								pixel_pointer[5] = colours[(character_value_ >> 2)&3];
								pixel_pointer[6] =
								pixel_pointer[7] = colours[(character_value_ >> 0)&3];
							}

							pixel_pointer += 8;
						}
					} else {
						character_code_ = pixel_data;
						character_colour_ = colour_data;
					}
				}

				// Keep counting columns even if sync or the colour burst have interceded.
				if(column_counter_ >= 0 && column_counter_ < columns_this_line_*2) {
					column_counter_++;
				}
			}
		}

		/*!
			Causes the 6560 to flush as much pending CRT and speaker communications as possible.
		*/
		inline void flush() {
			update_audio();
			audio_queue_.perform();
		}

		/*!
			Writes to a 6560 register.
		*/
		void write(int address, uint8_t value) {
			address &= 0xf;
			registers_.direct_values[address] = value;
			switch(address) {
				case 0x0:
					registers_.interlaced = !!(value&0x80) && timing_.supports_interlacing;
					registers_.first_column_location = value & 0x7f;
				break;

				case 0x1:
					registers_.first_row_location = value;
				break;

				case 0x2:
					registers_.number_of_columns = value & 0x7f;
					registers_.video_matrix_start_address = static_cast<uint16_t>((registers_.video_matrix_start_address & 0x3c00) | ((value & 0x80) << 2));
				break;

				case 0x3:
					registers_.number_of_rows = (value >> 1)&0x3f;
					registers_.tall_characters = !!(value&0x01);
				break;

				case 0x5:
					registers_.character_cell_start_address = static_cast<uint16_t>((value & 0x0f) << 10);
					registers_.video_matrix_start_address = static_cast<uint16_t>((registers_.video_matrix_start_address & 0x0200) | ((value & 0xf0) << 6));
				break;

				case 0xa:
				case 0xb:
				case 0xc:
				case 0xd:
					update_audio();
					audio_generator_.set_control(address - 0xa, value);
				break;

				case 0xe:
					update_audio();
					registers_.auxiliary_colour = colours_[value >> 4];
					audio_generator_.set_volume(value & 0xf);
				break;

				case 0xf: {
					uint16_t new_border_colour = colours_[value & 0x07];
					if(this_state_ == State::Border && new_border_colour != registers_.borderColour) {
						output_border(cycles_in_state_ * 4);
						cycles_in_state_ = 0;
					}
					registers_.invertedCells = !((value >> 3)&1);
					registers_.borderColour = new_border_colour;
					registers_.backgroundColour = colours_[value >> 4];
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
		uint8_t read(int address) {
			address &= 0xf;
			switch(address) {
				default: return registers_.direct_values[address];
				case 0x03: return static_cast<uint8_t>(raster_value() << 7) | (registers_.direct_values[3] & 0x7f);
				case 0x04: return (raster_value() >> 1) & 0xff;
			}
		}

	private:
		BusHandler &bus_handler_;
		Outputs::CRT::CRT crt_;

		Concurrency::DeferringAsyncTaskQueue audio_queue_;
		AudioGenerator audio_generator_;
		Outputs::Speaker::LowpassSpeaker<AudioGenerator, false> speaker_;

		Cycles cycles_since_speaker_update_;
		void update_audio() {
			speaker_.run_for(audio_queue_, Cycles(cycles_since_speaker_update_.divide(Cycles(4))));
		}

		// register state
		struct {
			bool interlaced = false, tall_characters = false;
			uint8_t first_column_location, first_row_location;
			uint8_t number_of_columns, number_of_rows;
			uint16_t character_cell_start_address, video_matrix_start_address;
			uint16_t backgroundColour, borderColour, auxiliary_colour;
			bool invertedCells = false;

			uint8_t direct_values[16];
		} registers_;

		// output state
		enum State {
			Sync, ColourBurst, Border, Pixels
		} this_state_, output_state_;
		int cycles_in_state_;

		// counters that cover an entire field
		int horizontal_counter_ = 0, vertical_counter_ = 0;
		const int lines_this_field() {
			// Necessary knowledge here: only the NTSC 6560 supports interlaced video.
			return registers_.interlaced ? (is_odd_frame_ ? 262 : 263) : timing_.lines_per_progressive_field;
		}
		const int raster_value() {
			const int bonus_line = (horizontal_counter_ + timing_.line_counter_increment_offset) / timing_.cycles_per_line;
			const int line = vertical_counter_ + bonus_line;
			const int final_line = lines_this_field();

			if(line < final_line)
				return line;

			if(is_odd_frame()) {
				return (horizontal_counter_ >= timing_.final_line_increment_position) ? 0 : final_line - 1;
			} else {
				return line % final_line;
			}
			// Cf. http://www.sleepingelephant.com/ipw-web/bulletin/bb/viewtopic.php?f=14&t=7237&start=15#p80737
		}
		bool is_odd_frame() {
			return is_odd_frame_ || !registers_.interlaced;
		}

		// latches dictating start and length of drawing
		bool vertical_drawing_latch_ = false, horizontal_drawing_latch_ = false;
		int rows_this_field_, columns_this_line_;

		// current drawing position counter
		int pixel_line_cycle_, column_counter_;
		int current_row_;
		uint16_t current_character_row_;
		uint16_t video_matrix_address_counter_, base_video_matrix_address_counter_;

		// data latched from the bus
		uint8_t character_code_, character_colour_, character_value_;

		bool is_odd_frame_ = false, is_odd_line_ = false;

		// lookup table from 6560 colour index to appropriate PAL/NTSC value
		uint16_t colours_[16];

		uint16_t *pixel_pointer;
		void output_border(int number_of_cycles) {
			uint16_t *colour_pointer = reinterpret_cast<uint16_t *>(crt_.begin_data(1));
			if(colour_pointer) *colour_pointer = registers_.borderColour;
			crt_.output_level(number_of_cycles);
		}

		struct {
			int cycles_per_line;
			int line_counter_increment_offset;
			int final_line_increment_position;
			int lines_per_progressive_field;
			bool supports_interlacing;
		} timing_;
		OutputMode output_mode_;
};

}
}

#endif /* _560_hpp */
