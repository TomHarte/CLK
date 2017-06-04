//
//  CSZX8081.m
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#import "CSZX8081.h"

#include "ZX8081.hpp"

#import "CSMachine+Subclassing.h"
#import "NSData+StdVector.h"
#import "NSBundle+DataResource.h"

@implementation CSZX8081 {
	ZX8081::Machine zx8081;
}

- (CRTMachine::Machine * const)machine {
	return &zx8081;
}

- (instancetype)init {
	self = [super init];
	if(self) {
		zx8081.set_rom(ZX8081::ROMType::ZX80, [self rom:@"zx80"].stdVector8);
		zx8081.set_rom(ZX8081::ROMType::ZX81, [self rom:@"zx81"].stdVector8);
	}
	return self;
}

- (NSData *)rom:(NSString *)name {
	return [[NSBundle mainBundle] dataForResource:name withExtension:@"rom" subdirectory:@"ROMImages/ZX8081"];
}

#pragma mark - Keyboard Mapping

- (void)clearAllKeys {
}

- (void)setKey:(uint16_t)key isPressed:(BOOL)isPressed {
}

- (NSString *)userDefaultsPrefix {	return @"zx8081";	}

@end
