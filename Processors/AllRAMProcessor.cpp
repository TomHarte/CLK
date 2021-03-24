//
//  AllRAMProcessor.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/05/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "AllRAMProcessor.hpp"

using namespace CPU;

AllRAMProcessor::AllRAMProcessor(std::size_t memory_size) :
	memory_(memory_size),
	traps_(memory_size, false),
	timestamp_(0) {}

void AllRAMProcessor::set_data_at_address(size_t start_address, std::size_t length, const uint8_t *data) {
	const size_t end_address = std::min(start_address + length, memory_.size());
	memcpy(&memory_[start_address], data, end_address - start_address);
}

void AllRAMProcessor::get_data_at_address(size_t start_address, std::size_t length, uint8_t *data) {
	const size_t end_address = std::min(start_address + length, memory_.size());
	memcpy(data, &memory_[start_address], end_address - start_address);
}

HalfCycles AllRAMProcessor::get_timestamp() {
	return timestamp_;
}

void AllRAMProcessor::set_trap_handler(TrapHandler *trap_handler) {
	trap_handler_ = trap_handler;
}

void AllRAMProcessor::add_trap_address(uint16_t address) {
	traps_[address] = true;
}
