//
//  CSElectron.m
//  Clock Signal
//
//  Created by Thomas Harte on 04/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import "CSElectron.h"

#include "Electron.hpp"
#include "TypedDynamicMachine.hpp"

@implementation CSElectron {
	Machine::TypedDynamicMachine<Electron::Machine> _electron;
}

- (instancetype)init {
	_electron = Machine::TypedDynamicMachine<Electron::Machine>(Electron::Machine::Electron());
	return [super initWithMachine:&_electron];
}

#pragma mark - ROM setting

- (NSString *)userDefaultsPrefix {	return @"electron";	}

@end
