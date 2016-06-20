//
//  MOS6532Bridge.m
//  Clock Signal
//
//  Created by Thomas Harte on 19/06/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import "MOS6532Bridge.h"
#include "6532.hpp"

class VanillaRIOT: public MOS::MOS6532<VanillaRIOT> {
};

@implementation MOS6532Bridge
{
	VanillaRIOT _riot;
}

- (void)setValue:(uint8_t)value forRegister:(NSUInteger)registerNumber
{
	_riot.set_register((int)registerNumber, value);
}

- (uint8_t)valueForRegister:(NSUInteger)registerNumber
{
	return _riot.get_register((int)registerNumber);
}

- (void)runForCycles:(NSUInteger)numberOfCycles
{
	_riot.run_for_cycles((int)numberOfCycles);
}

@end
