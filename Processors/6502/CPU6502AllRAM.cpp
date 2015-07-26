//
//  CPU6502AllRAM.cpp
//  CLK
//
//  Created by Thomas Harte on 13/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

#include "CPU6502AllRAM.hpp"
#include <algorithm>
#include <string.h>

using namespace CPU6502;

AllRAMProcessor::AllRAMProcessor()
{
	reset();
}

void AllRAMProcessor::perform_bus_operation(CPU6502::BusOperation operation, uint16_t address, uint8_t *value)
{
	if(isReadOperation(operation)) {
		*value = _memory[address];
	} else {
		_memory[address] = *value;
	}
}

void AllRAMProcessor::set_data_at_address(uint16_t startAddress, size_t length, const uint8_t *data)
{
	size_t endAddress = std::min(startAddress + length, (size_t)65536);
	memcpy(&_memory[startAddress], data, endAddress - startAddress);
}
