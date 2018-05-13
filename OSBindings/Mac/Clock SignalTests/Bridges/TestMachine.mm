//
//  TestMachine.m
//  Clock Signal
//
//  Created by Thomas Harte on 03/06/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#import "TestMachine.h"
#import "TestMachine+ForSubclassEyesOnly.h"
#include "AllRAMProcessor.hpp"

@interface CSTestMachine ()
- (void)testMachineDidTrapAtAddress:(uint16_t)address;
@end

#pragma mark - C++ delegate handlers

class MachineTrapHandler: public CPU::AllRAMProcessor::TrapHandler {
	public:
		MachineTrapHandler(CSTestMachine *targetMachine) : target_(targetMachine) {}

		void processor_did_trap(CPU::AllRAMProcessor &, uint16_t address) {
			[target_ testMachineDidTrapAtAddress:address];
		}

	private:
		CSTestMachine *target_;
};

#pragma mark - The test machine

@implementation CSTestMachine {
	MachineTrapHandler *_cppTrapHandler;
}

- (instancetype)init {
	if(self = [super init]) {
		_cppTrapHandler = new MachineTrapHandler(self);
	}
	return self;
}

- (void)dealloc {
	delete _cppTrapHandler;
}

- (void)addTrapAddress:(uint16_t)trapAddress {
	self.processor->set_trap_handler(_cppTrapHandler);
	self.processor->add_trap_address(trapAddress);
}

- (void)testMachineDidTrapAtAddress:(uint16_t)address {
	[self.trapHandler testMachine:self didTrapAtAddress:address];
}

@end
