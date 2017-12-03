//
//  CSMSX.m
//  Clock Signal
//
//  Created by Thomas Harte on 03/12/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#import "CSMSX.h"

#include "MSX.hpp"
#include "TypedDynamicMachine.hpp"

@implementation CSMSX {
	Machine::TypedDynamicMachine<MSX::Machine> _msx;
}

- (instancetype)init {
	_msx = Machine::TypedDynamicMachine<MSX::Machine>(MSX::Machine::MSX());
	return [super initWithMachine:&_msx];
}

- (NSString *)userDefaultsPrefix {	return @"MSX";	}

@end
