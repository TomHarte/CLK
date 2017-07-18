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
}

TZX::TZX(const char *file_name) :
	Storage::FileHolder(file_name),
	next_is_high_(false) {

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
}

void TZX::virtual_reset() {
	clear();
	set_is_at_end(false);
	next_is_high_ = false;
	fseek(file_, 0x0a, SEEK_SET);
}

void TZX::get_next_pulses() {
	while(empty()) {
		uint8_t chunk_id = (uint8_t)fgetc(file_);
		if(feof(file_)) {
			set_is_at_end(true);
			return;
		}

		switch(chunk_id) {
			case 0x10:	get_standard_speed_data_block();	break;
			case 0x11:	get_turbo_speed_data_block();		break;
			case 0x12:	get_pure_tone_data_block();			break;
			case 0x13:	get_pulse_sequence();				break;
			case 0x19:	get_generalised_data_block();		break;

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
	emplace_back(Tape::Pulse::Zero, Storage::Time((unsigned int)pause_after_block, 1000u));

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
	for(int c = 0; c < output_symbols; c++) {
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
				case 1: next_is_high_ ^= true;	break;
				case 2: next_is_high_ = false;	break;
				case 3: next_is_high_ = true;	break;
			}

			// Output waves.
			for(auto length : symbol.pulse_lengths) {
				if(!length) break;
				post_pulse(length);
			}
		}
	}
}

void TZX::post_pulse(unsigned int length) {
	emplace_back(next_is_high_ ? Tape::Pulse::High : Tape::Pulse::Low, Storage::Time(length, StandardTZXClock));
	next_is_high_ ^= true;
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
	__unused uint16_t length_of_pilot_pulse = fgetc16le();
	__unused uint16_t length_of_sync_first_pulse = fgetc16le();
	__unused uint16_t length_of_sync_second_pulse = fgetc16le();
	__unused uint16_t length_of_zero_bit_pulse = fgetc16le();
	__unused uint16_t length_of_one_bit_pulse = fgetc16le();
	__unused uint16_t length_of_pilot_tone = fgetc16le();
	__unused uint8_t number_of_bits_in_final_byte = (uint8_t)fgetc(file_);
	__unused uint16_t pause_after_block = fgetc16le();
	uint16_t data_length = fgetc16le();

	// TODO output as described
	fseek(file_, data_length, SEEK_CUR);
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
