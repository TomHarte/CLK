//
//  ZX80O.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "ZX80O.hpp"

using namespace Storage::Tape;

ZX80O::ZX80O(const char *file_name) :
	Storage::FileHolder(file_name) {

	// Files can be no longer than 16 kb
	if(file_stats_.st_size > 16384) throw ErrorNotZX80O;

	// skip the system area
	fseek(file_, 8, SEEK_SET);

	// read the pointer to VARS and the alleged pointer to end of file
	uint16_t vars = fgetc16le();
	end_of_file_ = fgetc16le();

	// VARs should be before end of file
	if(vars > end_of_file_) throw ErrorNotZX80O;

	// end of file should be no further than the actual file size
	if(end_of_file_ - 0x4000 > file_stats_.st_size) throw ErrorNotZX80O;

	// TODO: does it make sense to inspect the tokenised BASIC?
	// It starts at 0x4028 and proceeds as [16-bit line number] [tokens] [0x76],
	// but I'm as yet unable to find documentation of the tokens.

	// then rewind and start again
	virtual_reset();
}

void ZX80O::virtual_reset() {
	fseek(file_, 0, SEEK_SET);
	is_past_silence_ = false;
	has_ended_final_byte_ = false;
	is_high_ = true;
	bit_pointer_ = wave_pointer_ = 0;
}

bool ZX80O::has_finished_data() {
	return (ftell(file_) == end_of_file_ - 0x4000) && !wave_pointer_ && !bit_pointer_;
}

bool ZX80O::is_at_end() {
	return has_finished_data() && has_ended_final_byte_;
}

Tape::Pulse ZX80O::virtual_get_next_pulse() {
	Tape::Pulse pulse;

	// Start with 1 second of silence.
	if(!is_past_silence_ || has_finished_data()) {
		pulse.type = Pulse::Type::Low;
		pulse.length.length = 5;
		pulse.length.clock_rate = 1;
		is_past_silence_ = true;
		has_ended_final_byte_ = has_finished_data();
		return pulse;
	}

	// For each byte, output 8 bits and then silence.
	if(!bit_pointer_ && !wave_pointer_) {
		byte_ = (uint8_t)fgetc(file_);
		if(has_finished_data())
			printf("");
		bit_pointer_ = 0;
		wave_pointer_ = 0;
	}

	if(!wave_pointer_) {
		// post-waves silence (here actually a pre-waves silence) is 1300µs
		pulse.length.length = 13;
		pulse.length.clock_rate = 10000;
		pulse.type = Pulse::Type::Low;

		wave_pointer_ ++;
	} else {
		// pulses are of length 150µs
		pulse.length.length = 3;
		pulse.length.clock_rate = 20000;

		if(is_high_) {
			pulse.type = Pulse::Type::High;
			is_high_ = false;
		} else {
			pulse.type = Pulse::Type::Low;
			is_high_ = true;

			// Bytes are stored MSB first.
			int wave_count = (byte_ & (0x80 >> bit_pointer_)) ? 9 : 4;
			wave_pointer_++;
			if(wave_pointer_ == wave_count + 1) {
				bit_pointer_ = (bit_pointer_ + 1)&7;
				wave_pointer_ = 0;
			}
		}
	}

	return pulse;
}
