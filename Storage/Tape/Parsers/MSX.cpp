//
//  MSX.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 26/12/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "MSX.hpp"

#include <algorithm>

using namespace Storage::Tape::MSX;

std::unique_ptr<Parser::FileSpeed> Parser::find_header(Storage::Tape::BinaryTapePlayer &tape_player) {
	if(!tape_player.get_motor_control()) {
		return nullptr;
	}

	/*
		"When 1,111 cycles have been found with less than 35 microseconds
		variation in their lengths a header has been located."
	*/
	bool last_level = tape_player.get_input();
	float low = std::numeric_limits<float>::max();
	float high = std::numeric_limits<float>::min();
	int samples = 0;
	while(!tape_player.get_tape()->is_at_end()) {
		float next_length = 0.0f;
		do {
			next_length += float(tape_player.get_cycles_until_next_event()) / float(tape_player.get_input_clock_rate());
			tape_player.run_for_input_pulse();
		} while(last_level == tape_player.get_input());
		last_level = tape_player.get_input();
		low = std::min(low, next_length);
		high = std::max(high, next_length);
		samples++;
		if(high - low > 0.000035f) {
			low = std::numeric_limits<float>::max();
			high = std::numeric_limits<float>::min();
			samples = 0;
		}
		if(samples == 1111*2) break;	// Cycles are read, not half-cycles.
	}

	if(tape_player.get_tape()->is_at_end()) return nullptr;

	/*
		"The next 256 cycles are then read (1B34H) and averaged to determine the cassette HI cycle length."
	*/
	float total_length = 0.0f;
	samples = 512;
	while(!tape_player.get_tape()->is_at_end()) {
		total_length += float(tape_player.get_cycles_until_next_event()) / float(tape_player.get_input_clock_rate());
		if(tape_player.get_input() != last_level) {
			samples--;
			if(!samples) break;
			last_level = tape_player.get_input();
		}
		tape_player.run_for_input_pulse();
	}

	if(tape_player.get_tape()->is_at_end()) return nullptr;

	/*
		This figure is multiplied by 1.5 and placed in LOWLIM where it defines the minimum acceptable length
		of a 0 start bit. The HI cycle length is placed in WINWID and will be used to discriminate
		between LO and HI cycles."
	*/
	total_length = total_length / 256.0f;			// To get the average, in microseconds.
	// To convert to the loop count format used by the MSX BIOS.
	uint8_t int_result = uint8_t(total_length / (0.00001145f * 0.75f));

	auto result = std::make_unique<FileSpeed>();
	result->minimum_start_bit_duration = int_result;
	result->low_high_disrimination_duration = (int_result * 3) >> 2;

	return result;
}

/*!
	Attempts to read the next byte from the cassette, with data encoded
	at the rate as defined by @c speed.

	Attempts exactly to duplicate the MSX's TAPIN function.

	@returns A value in the range 0-255 if a byte is found before the end of the tape;
		-1 otherwise.
*/
int Parser::get_byte(const FileSpeed &speed, Storage::Tape::BinaryTapePlayer &tape_player) {
	if(!tape_player.get_motor_control()) {
		return -1;
	}

	/*
		"The cassette is first read continuously until a start bit is found.
		This is done by locating a negative transition, measuring the following
		cycle length (1B1FH) and comparing this to see if it is greater than LOWLIM."

		... but I don't buy that, as it makes the process overly dependent on phase.
		So I'm going to look for the next two consecutive pulses that are each big
		enough to be half of a zero.
	*/
	const float minimum_start_bit_duration = float(speed.minimum_start_bit_duration) * 0.00001145f * 0.5f;
	int input = 0;
	while(!tape_player.get_tape()->is_at_end()) {
		// Find next transition.
		bool level = tape_player.get_input();
		float duration = 0.0;
		while(level == tape_player.get_input()) {
			duration += float(tape_player.get_cycles_until_next_event()) / float(tape_player.get_input_clock_rate());
			tape_player.run_for_input_pulse();
		}

		input = (input << 1);
		if(duration >= minimum_start_bit_duration) input |= 1;
		if((input&3) == 3) break;
	}

	/*
		"Each of the eight data bits is then read by counting the number of transitions within
		a fixed period of time (1B03H). If zero or one transitions are found it is a 0 bit, if two
		or three are found it is a 1 bit.  If more than three transitions are found the routine
		terminates with Flag C as this is presumed to be a hardware error of some sort. "
	*/
	int result = 0;
	const int cycles_per_window = int(
		0.5f +
		float(speed.low_high_disrimination_duration) *
		0.0000173f *
		float(tape_player.get_input_clock_rate())
	);
	int bits_left = 8;
	bool level = tape_player.get_input();
	while(!tape_player.get_tape()->is_at_end() && bits_left--) {
		// Count number of transitions within cycles_per_window.
		int transitions = 0;
		int cycles_remaining = cycles_per_window;
		while(!tape_player.get_tape()->is_at_end() && cycles_remaining) {
			const int cycles_until_next_event = int(tape_player.get_cycles_until_next_event());
			const int cycles_to_run_for = std::min(cycles_until_next_event, cycles_remaining);

			cycles_remaining -= cycles_to_run_for;
			tape_player.run_for(Cycles(cycles_to_run_for));

			if(level != tape_player.get_input()) {
				level = tape_player.get_input();
				transitions++;
			}
		}

		if(tape_player.get_tape()->is_at_end()) return -1;

		int next_bit = 0;
		switch(transitions) {
			case 0: case 1:
				next_bit = 0x00;
			break;
			case 2: case 3:
				next_bit = 0x80;
			break;
			default:
			return -1;
		}
		result = (result >> 1) | next_bit;

		/*
			"After the value of each bit has been determined a further one or two transitions are read (1B23H)
			to retain synchronization. With an odd transition count one more will be read, with an even
			transition count two more."
		*/
		int required_transitions = 2 - (transitions&1);
		while(!tape_player.get_tape()->is_at_end()) {
			tape_player.run_for_input_pulse();
			if(level != tape_player.get_input()) {
				level = tape_player.get_input();
				required_transitions--;
				if(!required_transitions) break;
			}
		}

		if(tape_player.get_tape()->is_at_end()) return -1;
	}
	return result;
}
