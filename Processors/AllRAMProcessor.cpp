//
//  AllRAMProcessor.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/05/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "AllRAMProcessor.hpp"

#include <algorithm>

using namespace CPU;

AllRAMProcessor::AllRAMProcessor(const size_t memory_size) :
	memory_(memory_size),
	traps_(memory_size, false),
	timestamp_(0) {}

void AllRAMProcessor::set_data_at_address(const size_t start_address, const size_t length, const uint8_t *const data) {
	const size_t end_address = std::min(start_address + length, memory_.size());
	std::copy_n(data, end_address - start_address, &memory_[start_address]);
}

void AllRAMProcessor::get_data_at_address(const size_t start_address, const size_t length, uint8_t *const data) {
	const size_t end_address = std::min(start_address + length, memory_.size());
	std::copy_n(&memory_[start_address], end_address - start_address, data);
}

HalfCycles AllRAMProcessor::get_timestamp() {
	return timestamp_;
}

void AllRAMProcessor::set_trap_handler(TrapHandler *const trap_handler) {
	trap_handler_ = trap_handler;
}

void AllRAMProcessor::add_trap_address(const uint16_t address) {
	traps_[address] = true;
}
