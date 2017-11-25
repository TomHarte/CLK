//
//  CSOric.m
//  Clock Signal
//
//  Created by Thomas Harte on 11/10/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import "CSOric.h"

#include "Oric.hpp"
#include "TypedDynamicMachine.hpp"

@implementation CSOric {
	Machine::TypedDynamicMachine<Oric::Machine> _oric;
}

- (instancetype)init {
	_oric = Machine::TypedDynamicMachine<Oric::Machine>(Oric::Machine::Oric());
	return [super initWithMachine:&_oric];
}

@end
