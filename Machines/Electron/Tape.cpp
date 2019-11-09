//
//  Tape.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/12/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "Tape.hpp"

using namespace Electron;

Tape::Tape() : TapePlayer(2000000) {
	shifter_.set_delegate(this);
}

void Tape::push_tape_bit(uint16_t bit) {
	data_register_ = static_cast<uint16_t>((data_register_ >> 1) | (bit << 10));

	if(input_.minimum_bits_until_full) input_.minimum_bits_until_full--;
	if(input_.minimum_bits_until_full == 8) interrupt_status_ &= ~Interrupt::ReceiveDataFull;
	if(!input_.minimum_bits_until_full) {
		if((data_register_&0x3) == 0x1) {
			interrupt_status_ |= Interrupt::ReceiveDataFull;
			if(is_in_input_mode_) input_.minimum_bits_until_full = 9;
		}
	}

	if(output_.bits_remaining_until_empty)	output_.bits_remaining_until_empty--;
	if(!output_.bits_remaining_until_empty)	interrupt_status_ |= Interrupt::TransmitDataEmpty;

	if(data_register_ == 0x3ff)	interrupt_status_ |= Interrupt::HighToneDetect;
	else						interrupt_status_ &= ~Interrupt::HighToneDetect;

	evaluate_interrupts();
}

void Tape::evaluate_interrupts() {
	if(last_posted_interrupt_status_ != interrupt_status_) {
		last_posted_interrupt_status_ = interrupt_status_;
		if(delegate_) delegate_->tape_did_change_interrupt_status(this);
	}
}

void Tape::clear_interrupts(uint8_t interrupts) {
	interrupt_status_ &= ~interrupts;
	evaluate_interrupts();
}

void Tape::set_is_in_input_mode(bool is_in_input_mode) {
	is_in_input_mode_ = is_in_input_mode;
}

void Tape::set_counter(uint8_t value) {
	output_.cycles_into_pulse = 0;
	output_.bits_remaining_until_empty = 0;
}

void Tape::set_data_register(uint8_t value) {
	data_register_ = static_cast<uint16_t>((value << 2) | 1);
	output_.bits_remaining_until_empty = 9;
}

uint8_t Tape::get_data_register() {
	return static_cast<uint8_t>(data_register_ >> 2);
}

void Tape::process_input_pulse(const Storage::Tape::Tape::Pulse &pulse) {
	shifter_.process_pulse(pulse);
}

void Tape::acorn_shifter_output_bit(int value) {
	push_tape_bit(static_cast<uint16_t>(value));
}

void Tape::run_for(const Cycles cycles) {
	if(is_enabled_) {
		if(is_in_input_mode_) {
			if(is_running_) {
				TapePlayer::run_for(cycles);
			}
		} else {
			output_.cycles_into_pulse += static_cast<unsigned int>(cycles.as_integral());
			while(output_.cycles_into_pulse > 1664) {	// 1664 = the closest you can get to 1200 baud if you're looking for something
				output_.cycles_into_pulse -= 1664;		// that divides the 125,000Hz clock that the sound divider runs off.
				push_tape_bit(1);
			}
		}
	}
}
