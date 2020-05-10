//
//  Commodore.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/11/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "Commodore.hpp"

#include <cstring>
#include "../../Data/Commodore.hpp"

using namespace Storage::Tape::Commodore;

Parser::Parser() :
	Storage::Tape::PulseClassificationParser<WaveType, SymbolType>() {}

/*!
	Advances to the next block on the tape, treating it as a header, then consumes, parses, and returns it.
	Returns @c nullptr if any wave-encoding level errors are encountered.
*/
std::unique_ptr<Header> Parser::get_next_header(const std::shared_ptr<Storage::Tape::Tape> &tape) {
	return duplicate_match<Header>(
		get_next_header_body(tape, true),
		get_next_header_body(tape, false)
	);
}

/*!
	Advances to the next block on the tape, treating it as data, then consumes, parses, and returns it.
	Returns @c nullptr if any wave-encoding level errors are encountered.
*/
std::unique_ptr<Data> Parser::get_next_data(const std::shared_ptr<Storage::Tape::Tape> &tape) {
	return duplicate_match<Data>(
		get_next_data_body(tape, true),
		get_next_data_body(tape, false)
	);
}

/*!
	Template for the logic in selecting which of two copies of something to consider authoritative,
	including setting the duplicate_matched flag.
*/
template<class ObjectType>
	std::unique_ptr<ObjectType> Parser::duplicate_match(std::unique_ptr<ObjectType> first_copy, std::unique_ptr<ObjectType> second_copy) {
	// if only one copy was parsed successfully, return it
	if(!first_copy) return second_copy;
	if(!second_copy) return first_copy;

	// if no copies were second_copy, return nullptr
	if(!first_copy && !second_copy) return nullptr;

	// otherwise plan to return either one with a correct check digit, doing a comparison with the other
	std::unique_ptr<ObjectType> *copy_to_return = &first_copy;
	if(!first_copy->parity_was_valid && second_copy->parity_was_valid) copy_to_return = &second_copy;

	(*copy_to_return)->duplicate_matched = true;
	if(first_copy->data.size() != second_copy->data.size())
		(*copy_to_return)->duplicate_matched = false;
	else
		(*copy_to_return)->duplicate_matched = !(memcmp(&first_copy->data[0], &second_copy->data[0], first_copy->data.size()));

	return std::move(*copy_to_return);
}

std::unique_ptr<Header> Parser::get_next_header_body(const std::shared_ptr<Storage::Tape::Tape> &tape, bool is_original) {
	auto header = std::make_unique<Header>();
	reset_error_flag();

	// find and proceed beyond lead-in tone
	proceed_to_symbol(tape, SymbolType::LeadIn);

	// look for landing zone
	proceed_to_landing_zone(tape, is_original);
	reset_parity_byte();

	// get header type
	uint8_t header_type = get_next_byte(tape);
	switch(header_type) {
		default:	header->type = Header::Unknown;					break;
		case 0x01:	header->type = Header::RelocatableProgram;		break;
		case 0x02:	header->type = Header::DataBlock;				break;
		case 0x03:	header->type = Header::NonRelocatableProgram;	break;
		case 0x04:	header->type = Header::DataSequenceHeader;		break;
		case 0x05:	header->type = Header::EndOfTape;				break;
	}

	// grab rest of data
	header->data.reserve(191);
	for(std::size_t c = 0; c < 191; c++) {
		header->data.push_back(get_next_byte(tape));
	}

	uint8_t parity_byte = get_parity_byte();
	header->parity_was_valid = get_next_byte(tape) == parity_byte;

	// parse if this is not pure data
	if(header->type != Header::DataBlock) {
		header->starting_address	= uint16_t(header->data[0] | (header->data[1] << 8));
		header->ending_address		= uint16_t(header->data[2] | (header->data[3] << 8));

		for(std::size_t c = 0; c < 16; c++) {
			header->raw_name.push_back(header->data[4 + c]);
		}
		header->name = Storage::Data::Commodore::petscii_from_bytes(&header->raw_name[0], 16, false);
	}

	if(get_error_flag()) return nullptr;
	return header;
}

void Header::serialise(uint8_t *target, uint16_t length) {
	switch(type) {
		default:							target[0] = 0xff;	break;
		case Header::RelocatableProgram:	target[0] = 0x01;	break;
		case Header::DataBlock:				target[0] = 0x02;	break;
		case Header::NonRelocatableProgram:	target[0] = 0x03;	break;
		case Header::DataSequenceHeader:	target[0] = 0x04;	break;
		case Header::EndOfTape:				target[0] = 0x05;	break;
	}

	std::memcpy(&target[1], data.data(), 191);
}

std::unique_ptr<Data> Parser::get_next_data_body(const std::shared_ptr<Storage::Tape::Tape> &tape, bool is_original) {
	auto data = std::make_unique<Data>();
	reset_error_flag();

	// find and proceed beyond lead-in tone to the next landing zone
	proceed_to_symbol(tape, SymbolType::LeadIn);
	proceed_to_landing_zone(tape, is_original);
	reset_parity_byte();

	// accumulate until the next non-word marker is hit
	while(!tape->is_at_end()) {
		SymbolType start_symbol = get_next_symbol(tape);
		if(start_symbol != SymbolType::Word) break;
		data->data.push_back(get_next_byte_contents(tape));
	}

	// the above has reead the parity byte to the end of the data; if it matched the calculated parity it'll now be zero
	data->parity_was_valid = !get_parity_byte();
	data->duplicate_matched = false;

	// remove the captured parity
	data->data.erase(data->data.end()-1);
	if(get_error_flag()) return nullptr;
	return data;
}

/*!
	Finds and completes the next landing zone.
*/
void Parser::proceed_to_landing_zone(const std::shared_ptr<Storage::Tape::Tape> &tape, bool is_original) {
	uint8_t landing_zone[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
	while(!tape->is_at_end()) {
		memmove(landing_zone, &landing_zone[1], sizeof(uint8_t) * 8);
		landing_zone[8] = get_next_byte(tape);

		bool is_landing_zone = true;
		for(int c = 0; c < 9; c++) {
			if(landing_zone[c] != ((is_original ? 0x80 : 0x00) | 0x9) - c) {
				is_landing_zone = false;
				break;
			}
		}
		if(is_landing_zone) break;
	}
}

/*!
	Swallows symbols until it reaches the first instance of the required symbol, swallows that
	and returns.
*/
void Parser::proceed_to_symbol(const std::shared_ptr<Storage::Tape::Tape> &tape, SymbolType required_symbol) {
	while(!tape->is_at_end()) {
		SymbolType symbol = get_next_symbol(tape);
		if(symbol == required_symbol) return;
	}
}

/*!
	Swallows the next byte; sets the error flag if it is not equal to @c value.
*/
void Parser::expect_byte(const std::shared_ptr<Storage::Tape::Tape> &tape, uint8_t value) {
	uint8_t next_byte = get_next_byte(tape);
	if(next_byte != value) set_error_flag();
}

void Parser::reset_parity_byte()			{ parity_byte_ = 0;		}
uint8_t Parser::get_parity_byte()			{ return parity_byte_;	}
void Parser::add_parity_byte(uint8_t byte)	{ parity_byte_ ^= byte;	}

/*!
	Proceeds to the next word marker then returns the result of @c get_next_byte_contents.
*/
uint8_t Parser::get_next_byte(const std::shared_ptr<Storage::Tape::Tape> &tape) {
	proceed_to_symbol(tape, SymbolType::Word);
	return get_next_byte_contents(tape);
}

/*!
	Reads the next nine symbols and applies a binary test to each to differentiate between ::One and not-::One.
	Returns a byte composed of the first eight of those as bits; sets the error flag if any symbol is not
	::One and not ::Zero, or if the ninth bit is not equal to the odd parity of the other eight.
*/
uint8_t Parser::get_next_byte_contents(const std::shared_ptr<Storage::Tape::Tape> &tape) {
	int byte_plus_parity = 0;
	int c = 9;
	while(c--) {
		SymbolType next_symbol = get_next_symbol(tape);
		if((next_symbol != SymbolType::One) && (next_symbol != SymbolType::Zero)) set_error_flag();
		byte_plus_parity = (byte_plus_parity >> 1) | (((next_symbol == SymbolType::One) ? 1 : 0) << 8);
	}

	int check = byte_plus_parity;
	check ^= (check >> 4);
	check ^= (check >> 2);
	check ^= (check >> 1);
	if((check&1) == (byte_plus_parity >> 8))
		set_error_flag();

	add_parity_byte(uint8_t(byte_plus_parity));
	return uint8_t(byte_plus_parity);
}

/*!
	Returns the result of two consecutive @c get_next_byte calls, arranged in little-endian format.
*/
uint16_t Parser::get_next_short(const std::shared_ptr<Storage::Tape::Tape> &tape) {
	uint16_t value = get_next_byte(tape);
	value |= get_next_byte(tape) << 8;
	return value;
}

/*!
	Per the contract with Analyser::Static::TapeParser; sums time across pulses. If this pulse
	indicates a high to low transition, inspects the time since the last transition, to produce
	a long, medium, short or unrecognised wave period.
*/
void Parser::process_pulse(const Storage::Tape::Tape::Pulse &pulse) {
	// The Complete Commodore Inner Space Anthology, P 97, gives half-cycle lengths of:
	// short: 182us		=>	0.000364s cycle
	// medium: 262us	=>	0.000524s cycle
	// long: 342us		=>	0.000684s cycle
	bool is_high = pulse.type == Storage::Tape::Tape::Pulse::High;
	if(!is_high && previous_was_high_) {
		if(wave_period_ >= 0.000764)		push_wave(WaveType::Unrecognised);
		else if(wave_period_ >= 0.000604)	push_wave(WaveType::Long);
		else if(wave_period_ >= 0.000444)	push_wave(WaveType::Medium);
		else if(wave_period_ >= 0.000284)	push_wave(WaveType::Short);
		else push_wave(WaveType::Unrecognised);

		wave_period_ = 0.0f;
	}

	wave_period_ += pulse.length.get<float>();
	previous_was_high_ = is_high;
}

/*!
	Per the contract with Analyser::Static::TapeParser; produces any of a word marker, an end-of-block marker,
	a zero, a one or a lead-in symbol based on the currently captured waves.
*/
void Parser::inspect_waves(const std::vector<WaveType> &waves) {
	if(waves.size() < 2) return;

	if(waves[0] == WaveType::Long && waves[1] == WaveType::Medium) {
		push_symbol(SymbolType::Word, 2);
		return;
	}

	if(waves[0] == WaveType::Long && waves[1] == WaveType::Short) {
		push_symbol(SymbolType::EndOfBlock, 2);
		return;
	}

	if(waves[0] == WaveType::Short && waves[1] == WaveType::Medium) {
		push_symbol(SymbolType::Zero, 2);
		return;
	}

	if(waves[0] == WaveType::Medium && waves[1] == WaveType::Short) {
		push_symbol(SymbolType::One, 2);
		return;
	}

	if(waves[0] == WaveType::Short) {
		push_symbol(SymbolType::LeadIn, 1);
		return;
	}

	// Otherwise, eject at least one wave as all options are exhausted.
	remove_waves(1);
}
