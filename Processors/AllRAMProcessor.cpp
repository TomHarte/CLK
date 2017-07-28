//
//  AllRAMProcessor.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/05/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "AllRAMProcessor.hpp"

using namespace CPU;

AllRAMProcessor::AllRAMProcessor(size_t memory_size) :
	memory_(memory_size),
	traps_(memory_size, false),
	timestamp_(0) {}

void AllRAMProcessor::set_data_at_address(uint16_t startAddress, size_t length, const uint8_t *data) {
	size_t endAddress = std::min(startAddress + length, (size_t)65536);
	memcpy(&memory_[startAddress], data, endAddress - startAddress);
}

void AllRAMProcessor::get_data_at_address(uint16_t startAddress, size_t length, uint8_t *data) {
	size_t endAddress = std::min(startAddress + length, (size_t)65536);
	memcpy(data, &memory_[startAddress], endAddress - startAddress);
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
