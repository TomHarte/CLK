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
	AmstradCPC::Machine _amstradCPC;
}

- (CRTMachine::Machine * const)machine {
	return &_amstradCPC;
}

- (NSString *)userDefaultsPrefix {	return @"amstradCPC";	}

@end
