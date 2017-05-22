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

AllRAMProcessor::AllRAMProcessor() : ::CPU::AllRAMProcessor(65536), delegate_(nullptr) {}

int AllRAMProcessor::perform_machine_cycle(const MachineCycle *cycle) {
	switch(cycle->operation) {
		case BusOperation::ReadOpcode:
//			printf("! %02x\n", memory_[*cycle->address]);
			check_address_for_trap(*cycle->address);
		case BusOperation::Read:
//			printf("r %04x [%02x] AF:%04x BC:%04x DE:%04x HL:%04x SP:%04x\n", *cycle->address, memory_[*cycle->address], get_value_of_register(CPU::Z80::Register::AF), get_value_of_register(CPU::Z80::Register::BC), get_value_of_register(CPU::Z80::Register::DE), get_value_of_register(CPU::Z80::Register::HL), get_value_of_register(CPU::Z80::Register::StackPointer));
			*cycle->value = memory_[*cycle->address];
		break;
		case BusOperation::Write:
//			printf("w %04x\n", *cycle->address);
			memory_[*cycle->address] = *cycle->value;
		break;

		case BusOperation::Internal:
		break;

		default:
			printf("???\n");
		break;
	}
	timestamp_ += cycle->length;

	if(delegate_ != nullptr) {
		delegate_->z80_all_ram_processor_did_perform_bus_operation(*this, cycle->operation, *cycle->address, *cycle->value, timestamp_);
	}

	return 0;
}
