//
//  TestMachineZ80.m
//  Clock Signal
//
//  Created by Thomas Harte on 16/05/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#import "TestMachineZ80.h"
#include "Z80AllRAM.hpp"

@interface CSTestMachineZ80 ()
- (void)testMachineDidTrapAtAddress:(uint16_t)address;
@end

#pragma mark - C++ trap handler

class MachineTrapHandler: public CPU::AllRAMProcessor::TrapHandler {
	public:
		MachineTrapHandler(CSTestMachineZ80 *targetMachine) : target_(targetMachine) {}

		void processor_did_trap(CPU::AllRAMProcessor &, uint16_t address) {
			[target_ testMachineDidTrapAtAddress:address];
		}

	private:
		CSTestMachineZ80 *target_;
};

#pragma mark - Register enum map

static CPU::Z80::Register registerForRegister(CSTestMachineZ80Register reg) {
	switch (reg) {
		case CSTestMachineZ80RegisterProgramCounter:	return CPU::Z80::Register::ProgramCounter;
		case CSTestMachineZ80RegisterStackPointer:		return CPU::Z80::Register::StackPointer;
		case CSTestMachineZ80RegisterC:					return CPU::Z80::Register::C;
		case CSTestMachineZ80RegisterE:					return CPU::Z80::Register::E;
		case CSTestMachineZ80RegisterDE:				return CPU::Z80::Register::DE;
	}
}

#pragma mark - Test class

@implementation CSTestMachineZ80 {
	CPU::Z80::AllRAMProcessor _processor;
	MachineTrapHandler *_cppTrapHandler;
}

#pragma mark - Lifecycle

- (instancetype)init {
	if(self = [super init]) {
		_cppTrapHandler = new MachineTrapHandler(self);
		_processor.set_trap_handler(_cppTrapHandler);
	}
	return self;
}

- (void)dealloc {
	delete _cppTrapHandler;
}

#pragma mark - Accessors

- (void)setData:(NSData *)data atAddress:(uint16_t)startAddress {
	_processor.set_data_at_address(startAddress, data.length, (const uint8_t *)data.bytes);
}

- (void)runForNumberOfCycles:(int)cycles {
	_processor.run_for_cycles(cycles);
}

- (void)setValue:(uint16_t)value forRegister:(CSTestMachineZ80Register)reg {
	_processor.set_value_of_register(registerForRegister(reg), value);
}

- (void)setValue:(uint8_t)value atAddress:(uint16_t)address {
	_processor.set_data_at_address(address, 1, &value);
}

- (uint8_t)valueAtAddress:(uint16_t)address {
	uint8_t value;
	_processor.get_data_at_address(address, 1, &value);
	return value;
}

- (uint16_t)valueForRegister:(CSTestMachineZ80Register)reg {
	return _processor.get_value_of_register(registerForRegister(reg));
}

- (void)addTrapAddress:(uint16_t)trapAddress {
	_processor.add_trap_address(trapAddress);
}

- (void)testMachineDidTrapAtAddress:(uint16_t)address {
	[self.trapHandler testMachine:self didTrapAtAddress:address];
}

@end
