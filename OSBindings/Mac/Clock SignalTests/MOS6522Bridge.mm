//
//  MOS6522Bridge.m
//  Clock Signal
//
//  Created by Thomas Harte on 18/06/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import "MOS6522Bridge.h"
#include "6522.hpp"

@class MOS6522Bridge;

class VanillaVIA: public MOS::MOS6522<VanillaVIA> {
	public:
		MOS6522Bridge *bridge;
		bool irq_line;

		void set_interrupt_status(bool new_status)
		{
			irq_line = new_status;
		}
};

@implementation MOS6522Bridge
{
	VanillaVIA _via;
}

- (instancetype)init
{
	self = [super init];
	if(self)
	{
		_via.bridge = self;
	}
	return self;
}

- (void)setValue:(uint8_t)value forRegister:(NSUInteger)registerNumber
{
	_via.set_register((int)registerNumber, value);
}

- (uint8_t)valueForRegister:(NSUInteger)registerNumber
{
	return _via.get_register((int)registerNumber);
}

- (void)runForHalfCycles:(NSUInteger)numberOfHalfCycles
{
	_via.run_for_half_cycles(numberOfHalfCycles);
}

@end
