//
//  CSAmstradCPC.m
//  Clock Signal
//
//  Created by Thomas Harte on 30/07/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#import "CSAmstradCPC.h"

#include "AmstradCPC.hpp"

@implementation CSAmstradCPC {
	std::unique_ptr<AmstradCPC::Machine> _amstradCPC;
}

- (instancetype)init {
	AmstradCPC::Machine *machine = AmstradCPC::Machine::AmstradCPC();

	self = [super initWithMachine:machine];
	if(self) {
		_amstradCPC.reset(machine);
	}
	return self;
}

- (NSString *)userDefaultsPrefix {	return @"amstradCPC";	}

@end
