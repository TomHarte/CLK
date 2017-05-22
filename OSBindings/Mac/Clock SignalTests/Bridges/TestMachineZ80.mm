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
		case CSTestMachineZ80RegisterAF:				return CPU::Z80::Register::AF;
		case CSTestMachineZ80RegisterA:					return CPU::Z80::Register::A;
		case CSTestMachineZ80RegisterF:					return CPU::Z80::Register::Flags;
		case CSTestMachineZ80RegisterBC:				return CPU::Z80::Register::BC;
		case CSTestMachineZ80RegisterB:					return CPU::Z80::Register::B;
		case CSTestMachineZ80RegisterC:					return CPU::Z80::Register::C;
		case CSTestMachineZ80RegisterDE:				return CPU::Z80::Register::DE;
		case CSTestMachineZ80RegisterD:					return CPU::Z80::Register::D;
		case CSTestMachineZ80RegisterE:					return CPU::Z80::Register::E;
		case CSTestMachineZ80RegisterHL:				return CPU::Z80::Register::HL;
		case CSTestMachineZ80RegisterH:					return CPU::Z80::Register::H;
		case CSTestMachineZ80RegisterL:					return CPU::Z80::Register::L;

		case CSTestMachineZ80RegisterAFDash:			return CPU::Z80::Register::AFDash;
		case CSTestMachineZ80RegisterBCDash:			return CPU::Z80::Register::BCDash;
		case CSTestMachineZ80RegisterDEDash:			return CPU::Z80::Register::DEDash;
		case CSTestMachineZ80RegisterHLDash:			return CPU::Z80::Register::HLDash;

		case CSTestMachineZ80RegisterIX:				return CPU::Z80::Register::IX;
		case CSTestMachineZ80RegisterIY:				return CPU::Z80::Register::IY;

		case CSTestMachineZ80RegisterI:					return CPU::Z80::Register::I;
		case CSTestMachineZ80RegisterR:					return CPU::Z80::Register::R;

		case CSTestMachineZ80RegisterIFF1:				return CPU::Z80::Register::IFF1;
		case CSTestMachineZ80RegisterIFF2:				return CPU::Z80::Register::IFF2;
		case CSTestMachineZ80RegisterIM:				return CPU::Z80::Register::IM;

		case CSTestMachineZ80RegisterProgramCounter:	return CPU::Z80::Register::ProgramCounter;
		case CSTestMachineZ80RegisterStackPointer:		return CPU::Z80::Register::StackPointer;
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
