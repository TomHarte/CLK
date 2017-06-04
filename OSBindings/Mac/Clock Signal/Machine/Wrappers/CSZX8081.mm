//
//  CSZX8081.m
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#import "CSZX8081.h"

#include "ZX8081.hpp"

@implementation CSZX8081 {
	ZX8081::Machine zx8081;
}

- (CRTMachine::Machine * const)machine {
	return &zx8081;
}

- (instancetype)init {
	self = [super init];
	if(self) {
	}
	return self;
}

#pragma mark - Keyboard Mapping

- (void)clearAllKeys {
}

- (void)setKey:(uint16_t)key isPressed:(BOOL)isPressed {
}

- (NSString *)userDefaultsPrefix {	return @"zx8081";	}

@end
