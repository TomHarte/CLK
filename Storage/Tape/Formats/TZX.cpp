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
	is_high_(false) {

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
			case 0x19:	get_generalised_data_block();	break;

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
				case 1: is_high_ ^= true;	break;
				case 2: is_high_ = true;	break;
				case 3: is_high_ = false;	break;
			}

			// Output waves.
			for(auto length : symbol.pulse_lengths) {
				if(!length) break;

				is_high_ ^= true;
				emplace_back(is_high_ ? Tape::Pulse::High : Tape::Pulse::Low, Storage::Time((unsigned int)length, StandardTZXClock));
			}
		}
	}
}
