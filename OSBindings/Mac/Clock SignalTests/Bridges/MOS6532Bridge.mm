//
//  MOS6532Bridge.m
//  Clock Signal
//
//  Created by Thomas Harte on 19/06/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#import "MOS6532Bridge.h"
#include "6532.hpp"

class VanillaRIOT: public MOS::MOS6532<VanillaRIOT> {
	public:
		uint8_t get_port_input(int port) {
			return input[port];
		}
		uint8_t input[2];
};

@implementation MOS6532Bridge {
	VanillaRIOT _riot;
}

- (void)setValue:(uint8_t)value forRegister:(NSUInteger)registerNumber {
	_riot.write((int)registerNumber, value);
}

- (uint8_t)valueForRegister:(NSUInteger)registerNumber {
	return _riot.read((int)registerNumber);
}

- (void)runForCycles:(NSUInteger)numberOfCycles {
	_riot.run_for(Cycles((int)numberOfCycles));
}

- (BOOL)irqLine {
	return _riot.get_inerrupt_line();
}

- (void)setPortAInput:(uint8_t)portAInput {
	_riot.input[0] = _portAInput = portAInput;
}

- (void)setPortBInput:(uint8_t)portBInput {
	_riot.input[1] = _portBInput = portBInput;
}

@end
