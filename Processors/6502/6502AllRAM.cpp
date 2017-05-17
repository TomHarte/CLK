//
//  6502AllRAM.cpp
//  CLK
//
//  Created by Thomas Harte on 13/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

#include "6502AllRAM.hpp"
#include <algorithm>
#include <string.h>

using namespace CPU::MOS6502;

AllRAMProcessor::AllRAMProcessor() : _timestamp(0) {
	set_power_on(false);
}

int AllRAMProcessor::perform_bus_operation(MOS6502::BusOperation operation, uint16_t address, uint8_t *value) {
	timestamp_++;

	if(isReadOperation(operation)) {
		*value = memory_[address];
	} else {
		memory_[address] = *value;
	}

	return 1;
}

void AllRAMProcessor::set_data_at_address(uint16_t startAddress, size_t length, const uint8_t *data) {
	size_t endAddress = std::min(startAddress + length, (size_t)65536);
	memcpy(&memory_[startAddress], data, endAddress - startAddress);
}

uint32_t AllRAMProcessor::get_timestamp() {
	return timestamp_;
}
