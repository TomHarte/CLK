//
//  Machine.m
//  CLK
//
//  Created by Thomas Harte on 29/06/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

#import "TestMachine6502.h"
#include <stdint.h>
#include "6502AllRAM.hpp"

const uint8_t CSTestMachine6502JamOpcode = CPU::MOS6502::JamOpcode;

#pragma mark - C++ jam handler

class MachineJamHandler: public CPU::MOS6502::AllRAMProcessor::JamHandler {
	public:
		MachineJamHandler(CSTestMachine6502 *targetMachine) : _targetMachine(targetMachine) {}

		void processor_did_jam(CPU::MOS6502::AllRAMProcessor::Processor *processor, uint16_t address) override {
			[_targetMachine.jamHandler testMachine:_targetMachine didJamAtAddress:address];
		}

	private:
		CSTestMachine6502 *_targetMachine;
};

#pragma mark - Register enum map

static CPU::MOS6502::Register registerForRegister(CSTestMachine6502Register reg) {
	switch (reg) {
		case CSTestMachine6502RegisterProgramCounter:		return CPU::MOS6502::Register::ProgramCounter;
		case CSTestMachine6502RegisterLastOperationAddress:	return CPU::MOS6502::Register::LastOperationAddress;
		case CSTestMachine6502RegisterFlags:				return CPU::MOS6502::Register::Flags;
		case CSTestMachine6502RegisterA:					return CPU::MOS6502::Register::A;
		case CSTestMachine6502RegisterX:					return CPU::MOS6502::Register::X;
		case CSTestMachine6502RegisterY:					return CPU::MOS6502::Register::Y;
		case CSTestMachine6502RegisterStackPointer:			return CPU::MOS6502::Register::S;
	}
}

#pragma mark - Test class

@implementation CSTestMachine6502 {
	CPU::MOS6502::AllRAMProcessor _processor;
	MachineJamHandler *_cppJamHandler;
}

#pragma mark - Lifecycle

- (instancetype)init {
	self = [super init];

	if(self) {
		_cppJamHandler = new MachineJamHandler(self);
		_processor.set_jam_handler(_cppJamHandler);
	}

	return self;
}

- (void)dealloc {
	delete _cppJamHandler;
}

#pragma mark - Accessors

- (uint8_t)valueForAddress:(uint16_t)address {
	uint8_t value;
	_processor.perform_bus_operation(CPU::MOS6502::BusOperation::Read, address, &value);
	return value;
}

- (void)setValue:(uint8_t)value forAddress:(uint16_t)address {
	_processor.perform_bus_operation(CPU::MOS6502::BusOperation::Write, address, &value);
}

- (void)setValue:(uint16_t)value forRegister:(CSTestMachine6502Register)reg {
	_processor.set_value_of_register(registerForRegister(reg), value);
}

- (uint16_t)valueForRegister:(CSTestMachine6502Register)reg {
	return _processor.get_value_of_register(registerForRegister(reg));
}

- (void)setData:(NSData *)data atAddress:(uint16_t)startAddress {
	_processor.set_data_at_address(startAddress, data.length, (const uint8_t *)data.bytes);
}

- (BOOL)isJammed {
	return _processor.is_jammed();
}

- (uint32_t)timestamp {
	return _processor.get_timestamp();
}

- (void)setIrqLine:(BOOL)irqLine {
	_irqLine = irqLine;
	_processor.set_irq_line(irqLine);
}

- (void)setNmiLine:(BOOL)nmiLine {
	_nmiLine = nmiLine;
	_processor.set_nmi_line(nmiLine);
}

#pragma mark - Actions

- (void)returnFromSubroutine {
	_processor.return_from_subroutine();
}

- (void)runForNumberOfCycles:(int)cycles {
	_processor.run_for_cycles(cycles);
}

@end
