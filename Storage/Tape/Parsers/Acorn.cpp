//
//  Acorn.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/11/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "Acorn.hpp"

using namespace Storage::Tape::Acorn;

namespace {
constexpr int PLLClockRate = 1920000;
}

Parser::Parser(): crc_(0x1021) {
	shifter_.set_delegate(this);
}

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
	crc_.add(uint8_t(value));
	return value;
}

unsigned int Parser::get_next_short(const std::shared_ptr<Storage::Tape::Tape> &tape) {
	unsigned int result = unsigned(get_next_byte(tape));
	result |= unsigned(get_next_byte(tape)) << 8;
	return result;
}

unsigned int Parser::get_next_word(const std::shared_ptr<Storage::Tape::Tape> &tape) {
	unsigned int result = get_next_short(tape);
	result |= get_next_short(tape) << 8;
	return result;
}

void Parser::reset_crc()	{	crc_.reset();				}
uint16_t Parser::get_crc()	{	return crc_.get_value();	}

void Parser::acorn_shifter_output_bit(int value) {
	push_symbol(value ? SymbolType::One : SymbolType::Zero);
}

void Parser::process_pulse(const Storage::Tape::Tape::Pulse &pulse) {
	shifter_.process_pulse(pulse);
}


Shifter::Shifter() :
	pll_(PLLClockRate / 4800, *this),
	was_high_(false),
	input_pattern_(0),
	delegate_(nullptr) {}

void Shifter::process_pulse(const Storage::Tape::Tape::Pulse &pulse) {
	pll_.run_for(Cycles(int(float(PLLClockRate) * pulse.length.get<float>())));

	bool is_high = pulse.type == Storage::Tape::Tape::Pulse::High;
	if(is_high != was_high_) {
		pll_.add_pulse();
	}
	was_high_ = is_high;
}

void Shifter::digital_phase_locked_loop_output_bit(int value) {
	input_pattern_ = ((input_pattern_ << 1) | unsigned(value)) & 0xf;
	switch(input_pattern_) {
		case 0x5:	delegate_->acorn_shifter_output_bit(0); input_pattern_ = 0;	break;
		case 0xf:	delegate_->acorn_shifter_output_bit(1); input_pattern_ = 0;	break;
		default:	break;
	}
}
