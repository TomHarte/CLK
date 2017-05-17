//
//  Z80AllRAM.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/05/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "Z80AllRAM.hpp"
#include <algorithm>

using namespace CPU::Z80;

AllRAMProcessor::AllRAMProcessor() {}

int AllRAMProcessor::perform_machine_cycle(const MachineCycle *cycle) {
	return 0;
}

void AllRAMProcessor::set_data_at_address(uint16_t startAddress, size_t length, const uint8_t *data) {
	size_t endAddress = std::min(startAddress + length, (size_t)65536);
	memcpy(&memory_[startAddress], data, endAddress - startAddress);
}

uint32_t AllRAMProcessor::get_timestamp() {
	return timestamp_;
}
