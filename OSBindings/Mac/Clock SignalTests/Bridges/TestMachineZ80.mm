//
//  TestMachineZ80.m
//  Clock Signal
//
//  Created by Thomas Harte on 16/05/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#import "TestMachineZ80.h"
#include "Z80AllRAM.hpp"
#import "TestMachine+ForSubclassEyesOnly.h"

@interface CSTestMachineZ80 ()
- (void)testMachineDidPerformBusOperation:(CPU::Z80::PartialMachineCycle::Operation)operation
	address:(uint16_t)address
	value:(uint8_t)value
	timeStamp:(HalfCycles)time_stamp;
@end

#pragma mark - C++ delegate handlers

class BusOperationHandler: public CPU::Z80::AllRAMProcessor::MemoryAccessDelegate {
	public:
		BusOperationHandler(CSTestMachineZ80 *targetMachine) : target_(targetMachine) {}

		void z80_all_ram_processor_did_perform_bus_operation(CPU::Z80::AllRAMProcessor &processor, CPU::Z80::PartialMachineCycle::Operation operation, uint16_t address, uint8_t value, HalfCycles time_stamp) {
			[target_ testMachineDidPerformBusOperation:operation address:address value:value timeStamp:time_stamp];
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

		case CSTestMachineZ80RegisterMemPtr:			return CPU::Z80::Register::MemPtr;
	}
}

#pragma mark - Capture class

@interface CSTestMachineZ80BusOperationCapture()
@property(nonatomic, assign) CSTestMachineZ80BusOperationCaptureOperation operation;
@property(nonatomic, assign) uint16_t address;
@property(nonatomic, assign) uint8_t value;
@property(nonatomic, assign) int timeStamp;
@end

@implementation CSTestMachineZ80BusOperationCapture

- (NSString *)description {
	NSString *opName = @"";
	switch(self.operation) {
		case CSTestMachineZ80BusOperationCaptureOperationReadOpcode:		opName = @"ro";		break;
		case CSTestMachineZ80BusOperationCaptureOperationRead:				opName = @"r";		break;
		case CSTestMachineZ80BusOperationCaptureOperationWrite:				opName = @"w";		break;
		case CSTestMachineZ80BusOperationCaptureOperationPortRead:			opName = @"i";		break;
		case CSTestMachineZ80BusOperationCaptureOperationPortWrite:			opName = @"o";		break;
		case CSTestMachineZ80BusOperationCaptureOperationInternalOperation:	opName = @"iop";	break;
	}
	return [NSString stringWithFormat:@"%@ %04x %02x [%d]", opName, self.address, self.value, self.timeStamp];
}

@end

#pragma mark - Test class

@implementation CSTestMachineZ80 {
	CPU::Z80::AllRAMProcessor *_processor;
	BusOperationHandler *_busOperationHandler;

	NSMutableArray<CSTestMachineZ80BusOperationCapture *> *_busOperationCaptures;
	int _timeSeekingReadOpcode;
}

#pragma mark - Lifecycle

- (instancetype)init {
	if(self = [super init]) {
		_processor = CPU::Z80::AllRAMProcessor::Processor();
		_processor->reset_power_on();
		_busOperationHandler = new BusOperationHandler(self);
		_busOperationCaptures = [[NSMutableArray alloc] init];
	}
	return self;
}

- (void)dealloc {
	delete _busOperationHandler;
}

#pragma mark - Accessors

- (void)setData:(NSData *)data atAddress:(uint16_t)startAddress {
	_processor->set_data_at_address(startAddress, data.length, (const uint8_t *)data.bytes);
}

- (void)runForNumberOfCycles:(int)cycles {
	_processor->run_for(Cycles(cycles));
}

- (void)setValue:(uint16_t)value forRegister:(CSTestMachineZ80Register)reg {
	_processor->set_value_of_register(registerForRegister(reg), value);
}

- (void)setValue:(uint8_t)value atAddress:(uint16_t)address {
	_processor->set_data_at_address(address, 1, &value);
}

- (uint8_t)valueAtAddress:(uint16_t)address {
	uint8_t value;
	_processor->get_data_at_address(address, 1, &value);
	return value;
}

- (uint16_t)valueForRegister:(CSTestMachineZ80Register)reg {
	return _processor->get_value_of_register(registerForRegister(reg));
}

- (BOOL)isHalted {
	return _processor->get_halt_line() ? YES : NO;
}

- (int)completedHalfCycles {
	return int(_processor->get_timestamp().as_integral());
}

- (void)setNmiLine:(BOOL)nmiLine {
	_nmiLine = nmiLine;
	_processor->set_non_maskable_interrupt_line(nmiLine ? true : false);
}

- (void)setIrqLine:(BOOL)irqLine {
	_irqLine = irqLine;
	_processor->set_interrupt_line(irqLine ? true : false);
}

- (void)setWaitLine:(BOOL)waitLine {
	_waitLine = waitLine;
	_processor->set_wait_line(waitLine ? true : false);
}

- (CPU::AllRAMProcessor *)processor {
	return _processor;
}

#pragma mark - Bus operation accumulation

- (void)setCaptureBusActivity:(BOOL)captureBusActivity {
	_captureBusActivity = captureBusActivity;
	_processor->set_memory_access_delegate(captureBusActivity ? _busOperationHandler : nullptr);
}

- (void)testMachineDidPerformBusOperation:(CPU::Z80::PartialMachineCycle::Operation)operation address:(uint16_t)address value:(uint8_t)value timeStamp:(HalfCycles)timeStamp {
	if(self.captureBusActivity) {
		CSTestMachineZ80BusOperationCapture *capture = [[CSTestMachineZ80BusOperationCapture alloc] init];
		switch(operation) {
			case CPU::Z80::PartialMachineCycle::Write:
				capture.operation = CSTestMachineZ80BusOperationCaptureOperationWrite;
			break;

			case CPU::Z80::PartialMachineCycle::Read:
				capture.operation = CSTestMachineZ80BusOperationCaptureOperationRead;
			break;

			case CPU::Z80::PartialMachineCycle::Refresh:
				capture.operation = CSTestMachineZ80BusOperationCaptureOperationReadOpcode;
			break;

			case CPU::Z80::PartialMachineCycle::Input:
				capture.operation = CSTestMachineZ80BusOperationCaptureOperationPortRead;
			break;

			case CPU::Z80::PartialMachineCycle::Output:
				capture.operation = CSTestMachineZ80BusOperationCaptureOperationPortWrite;
			break;

			case CPU::Z80::PartialMachineCycle::Internal:
				capture.operation = CSTestMachineZ80BusOperationCaptureOperationInternalOperation;
			break;

			default: return;
		}
		capture.address = address;
		capture.value = value;
		capture.timeStamp = int(timeStamp.as_integral());

		[_busOperationCaptures addObject:capture];
	}
}

- (NSArray<CSTestMachineZ80BusOperationCapture *> *)busOperationCaptures {
	return [_busOperationCaptures copy];
}

@end
