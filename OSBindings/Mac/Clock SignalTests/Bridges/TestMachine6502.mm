//
//  Machine.m
//  CLK
//
//  Created by Thomas Harte on 29/06/2015.
//  Copyright 2015 Thomas Harte. All rights reserved.
//

#import "TestMachine6502.h"
#include <stdint.h>
#include "../../../../Processors/6502/AllRAM/6502AllRAM.hpp"
#import "TestMachine+ForSubclassEyesOnly.h"

const uint8_t CSTestMachine6502JamOpcode = CPU::MOS6502::JamOpcode;

#pragma mark - Register enum map

static CPU::MOS6502::Register registerForRegister(CSTestMachine6502Register reg) {
	switch (reg) {
		case CSTestMachine6502RegisterProgramCounter:		return CPU::MOS6502::Register::ProgramCounter;
		case CSTestMachine6502RegisterLastOperationAddress:	return CPU::MOS6502::Register::LastOperationAddress;
		case CSTestMachine6502RegisterFlags:				return CPU::MOS6502::Register::Flags;
		case CSTestMachine6502RegisterA:					return CPU::MOS6502::Register::A;
		case CSTestMachine6502RegisterX:					return CPU::MOS6502::Register::X;
		case CSTestMachine6502RegisterY:					return CPU::MOS6502::Register::Y;
		case CSTestMachine6502RegisterStackPointer:			return CPU::MOS6502::Register::StackPointer;
		case CSTestMachine6502RegisterEmulationFlag:		return CPU::MOS6502::Register::EmulationFlag;
		case CSTestMachine6502RegisterDataBank:				return CPU::MOS6502::Register::DataBank;
		case CSTestMachine6502RegisterProgramBank:			return CPU::MOS6502::Register::ProgramBank;
		case CSTestMachine6502RegisterDirect:				return CPU::MOS6502::Register::Direct;
	}
}

#pragma mark - Test class

@implementation CSTestMachine6502 {
	CPU::MOS6502::AllRAMProcessor *_processor;
}

#pragma mark - Lifecycle

- (instancetype)initWithProcessor:(CSTestMachine6502Processor)processor hasCIAs:(BOOL)hasCIAs {
	self = [super init];

	if(self) {
		switch(processor) {
			case CSTestMachine6502ProcessorNES6502:
				_processor = CPU::MOS6502::AllRAMProcessor::Processor(CPU::MOS6502Esque::Type::TNES6502, hasCIAs);
			break;
			case CSTestMachine6502Processor6502:
				_processor = CPU::MOS6502::AllRAMProcessor::Processor(CPU::MOS6502Esque::Type::T6502, hasCIAs);
			break;
			case CSTestMachine6502Processor65C02:
				_processor = CPU::MOS6502::AllRAMProcessor::Processor(CPU::MOS6502Esque::Type::TWDC65C02, hasCIAs);
			break;
			case CSTestMachine6502Processor65816:
				_processor = CPU::MOS6502::AllRAMProcessor::Processor(CPU::MOS6502Esque::Type::TWDC65816, hasCIAs);
		}
	}

	return self;
}

- (nonnull instancetype)initWithProcessor:(CSTestMachine6502Processor)processor {
	return [self initWithProcessor:processor hasCIAs:NO];
}

- (void)dealloc {
	delete _processor;
}

#pragma mark - Accessors

- (uint8_t)valueForAddress:(uint32_t)address {
	uint8_t value;
	_processor->get_data_at_address(address, 1, &value);
	return value;
}

- (void)setValue:(uint8_t)value forAddress:(uint32_t)address {
	_processor->set_data_at_address(address, 1, &value);
}

- (void)setValue:(uint16_t)value forRegister:(CSTestMachine6502Register)reg {
	_processor->set_value_of(registerForRegister(reg), value);
}

- (uint16_t)valueForRegister:(CSTestMachine6502Register)reg {
	return _processor->value_of(registerForRegister(reg));
}

- (void)setData:(NSData *)data atAddress:(uint32_t)startAddress {
	_processor->set_data_at_address(startAddress, data.length, (const uint8_t *)data.bytes);
}

- (nonnull NSData *)dataAtAddress:(uint32_t)address length:(uint32_t)length {
	NSMutableData *data = [[NSMutableData alloc] initWithLength:length];
	_processor->get_data_at_address(address, length, (uint8_t *)data.mutableBytes);
	return data;
}

- (BOOL)isJammed {
	return _processor->is_jammed();
}

- (uint32_t)timestamp {
	return uint32_t(_processor->get_timestamp().as_integral());
}

- (void)setIrqLine:(BOOL)irqLine {
	_irqLine = irqLine;
	_processor->set_irq_line(irqLine);
}

- (void)setNmiLine:(BOOL)nmiLine {
	_nmiLine = nmiLine;
	_processor->set_nmi_line(nmiLine);
}

- (CPU::AllRAMProcessor *)processor {
	return _processor;
}

#pragma mark - Actions

- (void)runForNumberOfCycles:(int)cycles {
	_processor->run_for(Cycles(cycles));
}

- (void)runForNumberOfInstructions:(int)instructions {
	_processor->run_for_instructions(instructions);
}

@end
