//
//  CSAmstradCPC.m
//  Clock Signal
//
//  Created by Thomas Harte on 30/07/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#import "CSAmstradCPC.h"

#include "AmstradCPC.hpp"

#import "CSMachine+Subclassing.h"
#import "NSData+StdVector.h"
#import "NSBundle+DataResource.h"

@implementation CSAmstradCPC {
	AmstradCPC::Machine _amstradCPC;
}

- (CRTMachine::Machine * const)machine {
	return &_amstradCPC;
}

- (instancetype)init {
	self = [super init];
	if(self) {
		NSDictionary *roms = @{
			@(AmstradCPC::ROMType::OS464) : @"os464",
			@(AmstradCPC::ROMType::OS664) : @"os664",
			@(AmstradCPC::ROMType::OS6128) : @"os6128",
			@(AmstradCPC::ROMType::BASIC464) : @"basic464",
			@(AmstradCPC::ROMType::BASIC664) : @"basic664",
			@(AmstradCPC::ROMType::BASIC6128) : @"basic6128",
			@(AmstradCPC::ROMType::AMSDOS) : @"amsdos",
		};

		for(NSNumber *key in roms.allKeys) {
			AmstradCPC::ROMType type = (AmstradCPC::ROMType)key.integerValue;
			NSString *name = roms[key];
			NSData *data = [self rom:name];
			if(data) {
				_amstradCPC.set_rom(type, data.stdVector8);
			} else {
				NSLog(@"Amstrad CPC ROM missing: %@", name);
			}
		}
	}
	return self;
}

- (NSData *)rom:(NSString *)name {
	return [[NSBundle mainBundle] dataForResource:name withExtension:@"rom" subdirectory:@"ROMImages/AmstradCPC"];
}

- (NSString *)userDefaultsPrefix {	return @"amstradCPC";	}

@end
