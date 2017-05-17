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

AllRAMProcessor::AllRAMProcessor() : ::CPU::AllRAMProcessor(65536) {}

int AllRAMProcessor::perform_bus_operation(MOS6502::BusOperation operation, uint16_t address, uint8_t *value) {
	timestamp_++;

	if(isReadOperation(operation)) {
		*value = memory_[address];
	} else {
		memory_[address] = *value;
	}

	return 1;
}
