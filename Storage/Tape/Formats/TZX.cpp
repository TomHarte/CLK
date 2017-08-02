//
//  TZX.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/07/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "TZX.hpp"

using namespace Storage::Tape;

namespace {
const unsigned int StandardTZXClock = 3500000;
const unsigned int TZXClockMSMultiplier = 3500;
}

TZX::TZX(const char *file_name) :
	Storage::FileHolder(file_name),
	current_level_(false) {

	// Check for signature followed by a 0x1a
	char identifier[7];
	char signature[] = "ZXTape!";
	fread(identifier, 1, strlen(signature), file_);
	if(memcmp(identifier, signature, strlen(signature))) throw ErrorNotTZX;
	if(fgetc(file_) != 0x1a) throw ErrorNotTZX;

	// Get version number
	uint8_t major_version = (uint8_t)fgetc(file_);
	uint8_t minor_version = (uint8_t)fgetc(file_);

	// Reject if an incompatible version
	if(major_version != 1 || minor_version > 20)  throw ErrorNotTZX;

	virtual_reset();
}

void TZX::virtual_reset() {
	clear();
	set_is_at_end(false);
	fseek(file_, 0x0a, SEEK_SET);

	// This is a workaround for arguably dodgy ZX80/ZX81 TZXs; they launch straight
	// into data but both machines require a gap before data begins. So impose
	// an initial gap, in the form of a very long wave.
	current_level_ = false;
	post_gap(500);
}

void TZX::get_next_pulses() {
	while(empty()) {
		uint8_t chunk_id = (uint8_t)fgetc(file_);
		if(feof(file_)) {
			set_is_at_end(true);
			return;
		}

//		printf("TZX %ld\n", ftell(file_));
		switch(chunk_id) {
			case 0x10:	get_standard_speed_data_block();	break;
			case 0x11:	get_turbo_speed_data_block();		break;
			case 0x12:	get_pure_tone_data_block();			break;
			case 0x13:	get_pulse_sequence();				break;
			case 0x19:	get_generalised_data_block();		break;
			case 0x20:	get_pause();						break;

			case 0x30: {
				// Text description. Ripe for ignoring.
				int length = fgetc(file_);
				fseek(file_, length, SEEK_CUR);
			} break;

			default:
				// In TZX each chunk has a different way of stating or implying its length,
				// so there is no route past an unimplemented chunk.
				printf("!!Unknown %02x!!", chunk_id);
				set_is_at_end(true);
			return;
		}
	}
}

void TZX::get_generalised_data_block() {
	uint32_t block_length = fgetc32le();
	long endpoint = ftell(file_) + (long)block_length;
	uint16_t pause_after_block = fgetc16le();

	uint32_t total_pilot_symbols = fgetc32le();
	uint8_t maximum_pulses_per_pilot_symbol = (uint8_t)fgetc(file_);
	uint8_t symbols_in_pilot_table = (uint8_t)fgetc(file_);

	uint32_t total_data_symbols = fgetc32le();
	uint8_t maximum_pulses_per_data_symbol = (uint8_t)fgetc(file_);
	uint8_t symbols_in_data_table = (uint8_t)fgetc(file_);

	get_generalised_segment(total_pilot_symbols, maximum_pulses_per_pilot_symbol, symbols_in_pilot_table, false);
	get_generalised_segment(total_data_symbols, maximum_pulses_per_data_symbol, symbols_in_data_table, true);
	post_gap(pause_after_block);

	// This should be unnecessary, but intends to preserve sanity.
	fseek(file_, endpoint, SEEK_SET);
}

void TZX::get_generalised_segment(uint32_t output_symbols, uint8_t max_pulses_per_symbol, uint8_t number_of_symbols, bool is_data) {
	if(!output_symbols) return;

	// Construct the symbol table.
	struct Symbol {
		uint8_t flags;
		std::vector<uint16_t> pulse_lengths;
	};
	std::vector<Symbol> symbol_table;
	for(int c = 0; c < number_of_symbols; c++) {
		Symbol symbol;
		symbol.flags = (uint8_t)fgetc(file_);
		for(int ic = 0; ic < max_pulses_per_symbol; ic++) {
			symbol.pulse_lengths.push_back(fgetc16le());
		}
		symbol_table.push_back(symbol);
	}

	// Hence produce the output.
	BitStream stream(file_, false);
	int base = 2;
	int bits = 1;
	while(base < number_of_symbols) {
		base <<= 1;
		bits++;
	}
	for(size_t c = 0; c < output_symbols; c++) {
		uint8_t symbol_value;
		int count;
		if(is_data) {
			symbol_value = stream.get_bits(bits);
			count = 1;
		} else {
			symbol_value = (uint8_t)fgetc(file_);
			count = fgetc16le();
		}
		if(symbol_value > number_of_symbols) {
			continue;
		}
		Symbol &symbol = symbol_table[symbol_value];

		while(count--) {
			// Mutate initial output level.
			switch(symbol.flags & 3) {
				case 0: break;
				case 1: current_level_ ^= true;	break;
				case 2: current_level_ = true;	break;
				case 3: current_level_ = false;	break;
			}

			// Output waves.
			for(auto length : symbol.pulse_lengths) {
				if(!length) break;
				post_pulse(length);
			}
		}
	}
}

void TZX::get_standard_speed_data_block() {
	__unused uint16_t pause_after_block = fgetc16le();
	uint16_t data_length = fgetc16le();
	if(!data_length) return;

	uint8_t first_byte = (uint8_t)fgetc(file_);
	__unused int pilot_tone_pulses = (first_byte < 128) ? 8063  : 3223;
	ungetc(first_byte, file_);

	// TODO: output pilot_tone_pulses pulses
	// TODO: output data_length bytes, in the Spectrum encoding
	fseek(file_, data_length, SEEK_CUR);
	// TODO: output a pause of length paused_after_block ms
}

void TZX::get_turbo_speed_data_block() {
	uint16_t length_of_pilot_pulse = fgetc16le();
	uint16_t length_of_sync_first_pulse = fgetc16le();
	uint16_t length_of_sync_second_pulse = fgetc16le();
	uint16_t length_of_zero_bit_pulse = fgetc16le();
	uint16_t length_of_one_bit_pulse = fgetc16le();
	uint16_t length_of_pilot_tone = fgetc16le();
	uint8_t number_of_bits_in_final_byte = (uint8_t)fgetc(file_);
	uint16_t pause_after_block = fgetc16le();
	long data_length = fgetc16le();
	data_length |= (long)(fgetc(file_) << 16);

	// Output pilot tone.
	while(length_of_pilot_tone--) {
		post_pulse(length_of_pilot_pulse);
	}

	// Output sync pulses.
	post_pulse(length_of_sync_first_pulse);
	post_pulse(length_of_sync_second_pulse);

	// Output data.
	while(data_length--) {
		uint8_t next_byte = (uint8_t)fgetc(file_);

		int c = data_length ? 8 : number_of_bits_in_final_byte;
		while(c--) {
			uint16_t pulse_length = (next_byte & 0x80) ? length_of_one_bit_pulse : length_of_zero_bit_pulse;
			next_byte <<= 1;

			post_pulse(pulse_length);
			post_pulse(pulse_length);
		}
	}

	// Output gap.
	post_gap(pause_after_block);
}

void TZX::get_pure_tone_data_block() {
	__unused uint16_t length_of_pulse = fgetc16le();
	__unused uint16_t nunber_of_pulses = fgetc16le();

	while(nunber_of_pulses--) post_pulse(length_of_pulse);
}

void TZX::get_pulse_sequence() {
	uint8_t number_of_pulses = (uint8_t)fgetc(file_);
	while(number_of_pulses--) {
		post_pulse(fgetc16le());
	}
}

void TZX::get_pause() {
	uint16_t duration = fgetc16le();
	if(!duration) {
		// TODO (maybe): post a 'pause the tape' suggestion
	} else {
		post_gap(duration);
	}
}

#pragma mark - Output

void TZX::post_pulse(unsigned int length) {
	post_pulse(Storage::Time(length, StandardTZXClock));
}

void TZX::post_gap(unsigned int milliseconds) {
	if(!milliseconds) return;
	if(milliseconds > 1 && !current_level_) {
		post_pulse(Storage::Time(TZXClockMSMultiplier, StandardTZXClock));
		post_pulse(Storage::Time((milliseconds - 1u)*TZXClockMSMultiplier, StandardTZXClock));
	} else {
		post_pulse(Storage::Time(milliseconds*TZXClockMSMultiplier, StandardTZXClock));
	}
}

void TZX::post_pulse(const Storage::Time &time) {
	emplace_back(current_level_ ? Tape::Pulse::High : Tape::Pulse::Low, time);
	current_level_ ^= true;
}
