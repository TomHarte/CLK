//
//  Machine.m
//  CLK
//
//  Created by Thomas Harte on 29/06/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

#import "TestMachine.h"
#include <stdint.h>
#include "CPU6502AllRAM.hpp"

const uint8_t CSTestMachineJamOpcode = CPU6502::JamOpcode;

class MachineJamHandler: public CPU6502::AllRAMProcessor::JamHandler {
	public:
		MachineJamHandler(CSTestMachine *targetMachine) {
			_targetMachine = targetMachine;
		}

		void processor_did_jam(CPU6502::AllRAMProcessor::Processor *processor, uint16_t address) override {
			[_targetMachine.jamHandler testMachine:_targetMachine didJamAtAddress:address];
		}

	private:
		CSTestMachine *_targetMachine;
};

@implementation CSTestMachine {
	CPU6502::AllRAMProcessor _processor;
	MachineJamHandler *_cppJamHandler;
}

- (uint8_t)valueForAddress:(uint16_t)address {
	uint8_t value;
	_processor.perform_bus_operation(CPU6502::BusOperation::Read, address, &value);
	return value;
}

- (void)setValue:(uint8_t)value forAddress:(uint16_t)address {
	_processor.perform_bus_operation(CPU6502::BusOperation::Write, address, &value);
}

- (void)returnFromSubroutine {
	_processor.return_from_subroutine();
}

- (CPU6502::Register)registerForRegister:(CSTestMachineRegister)reg {
	switch (reg) {
		case CSTestMachineRegisterProgramCounter:		return CPU6502::Register::ProgramCounter;
		case CSTestMachineRegisterLastOperationAddress:	return CPU6502::Register::LastOperationAddress;
		case CSTestMachineRegisterFlags:				return CPU6502::Register::Flags;
		case CSTestMachineRegisterA:					return CPU6502::Register::A;
		case CSTestMachineRegisterStackPointer:			return CPU6502::Register::S;
		default: break;
	}
}

- (void)setValue:(uint16_t)value forRegister:(CSTestMachineRegister)reg {
	_processor.set_value_of_register([self registerForRegister:reg], value);
}

- (uint16_t)valueForRegister:(CSTestMachineRegister)reg {
	return _processor.get_value_of_register([self registerForRegister:reg]);
}

- (void)setData:(NSData *)data atAddress:(uint16_t)startAddress {
	_processor.set_data_at_address(startAddress, data.length, (const uint8_t *)data.bytes);
}

//- (void)reset {
//	_processor.reset();
//}

- (void)runForNumberOfCycles:(int)cycles {
	_processor.run_for_cycles(cycles);
}

- (BOOL)isJammed {
	return _processor.is_jammed();
}

- (instancetype)init {
	self = [super init];

	if (self) {
		_cppJamHandler = new MachineJamHandler(self);
		_processor.set_jam_handler(_cppJamHandler);
	}

	return self;
}

- (void)dealloc {
	delete _cppJamHandler;
}

- (uint32_t)timestamp {
	return _processor.get_timestamp();
}

@end
