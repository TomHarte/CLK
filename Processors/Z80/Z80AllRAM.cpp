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

AllRAMProcessor::AllRAMProcessor() : ::CPU::AllRAMProcessor(65536) {}

int AllRAMProcessor::perform_machine_cycle(const MachineCycle *cycle) {
	switch(cycle->operation) {
		case BusOperation::ReadOpcode:
		case BusOperation::Read:
			printf("r %04x\n", *cycle->address);
			*cycle->value = memory_[*cycle->address];
		break;
		case BusOperation::Write:
			printf("w %04x\n", *cycle->address);
			memory_[*cycle->address] = *cycle->value;
		break;

		case BusOperation::Internal:
		break;

		default:
			printf("???\n");
		break;
	}
	return 0;
}
