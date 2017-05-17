//
//  TestMachineZ80.m
//  Clock Signal
//
//  Created by Thomas Harte on 16/05/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#import "TestMachineZ80.h"
#include "Z80AllRAM.hpp"

static CPU::Z80::Register registerForRegister(CSTestMachineZ80Register reg) {
	switch (reg) {
		case CSTestMachineZ80RegisterProgramCounter:	return CPU::Z80::Register::ProgramCounter;
		case CSTestMachineZ80RegisterStackPointer:		return CPU::Z80::Register::StackPointer;
	}
}

@implementation CSTestMachineZ80 {
	CPU::Z80::AllRAMProcessor _processor;
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

- (uint16_t)valueForRegister:(CSTestMachineZ80Register)reg {
	return _processor.get_value_of_register(registerForRegister(reg));
}

@end
