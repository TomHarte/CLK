//
//  Acorn.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/11/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Acorn.hpp"

using namespace Storage::Tape::Acorn;

namespace {
const int PLLClockRate = 1920000;
}

Parser::Parser() :
	::Storage::Tape::PLLParser<SymbolType>(PLLClockRate, PLLClockRate / 4800, PLLClockRate / 24000),
	crc_(0x1021, 0x0000) {}

int Parser::get_next_bit(const std::shared_ptr<Storage::Tape::Tape> &tape) {
	SymbolType symbol = get_next_symbol(tape);
	return (symbol == SymbolType::One) ? 1 : 0;
}

int Parser::get_next_byte(const std::shared_ptr<Storage::Tape::Tape> &tape) {
	int value = 0;
	int c = 8;
	if(get_next_bit(tape)) {
		set_error_flag();
		return -1;
	}
	while(c--) {
		value = (value >> 1) | (get_next_bit(tape) << 7);
	}
	if(!get_next_bit(tape)) {
		set_error_flag();
		return -1;
	}
	crc_.add((uint8_t)value);
	return value;
}

int Parser::get_next_short(const std::shared_ptr<Storage::Tape::Tape> &tape) {
	int result = get_next_byte(tape);
	result |= get_next_byte(tape) << 8;
	return result;
}

int Parser::get_next_word(const std::shared_ptr<Storage::Tape::Tape> &tape) {
	int result = get_next_short(tape);
	result |= get_next_short(tape) << 8;
	return result;
}

void Parser::reset_crc()	{	crc_.reset();				}
uint16_t Parser::get_crc()	{	return crc_.get_value();	}

bool Parser::did_update_shifter(int new_value, int length) {
	if(length < 4) return false;

	switch(new_value & 0xf) {
		case 0x5:	printf("0"); push_symbol(SymbolType::Zero);	return true;
		case 0xf:	printf("1"); push_symbol(SymbolType::One);	return true;
		default:
			printf("?");
		return false;
	}
}
