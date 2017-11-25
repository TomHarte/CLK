//
//  CSAmstradCPC.m
//  Clock Signal
//
//  Created by Thomas Harte on 30/07/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#import "CSAmstradCPC.h"

#include "AmstradCPC.hpp"
#include "TypedDynamicMachine.hpp"

@implementation CSAmstradCPC {
	Machine::TypedDynamicMachine<AmstradCPC::Machine> _amstradCPC;
}

- (instancetype)init {
	_amstradCPC = Machine::TypedDynamicMachine<AmstradCPC::Machine>(AmstradCPC::Machine::AmstradCPC());
	return [super initWithMachine:&_amstradCPC];
}

- (NSString *)userDefaultsPrefix {	return @"amstradCPC";	}

@end
