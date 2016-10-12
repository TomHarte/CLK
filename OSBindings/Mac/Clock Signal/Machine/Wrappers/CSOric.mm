//
//  CSOric.m
//  Clock Signal
//
//  Created by Thomas Harte on 11/10/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import "CSOric.h"

#include "Oric.hpp"
#include "StaticAnalyser.hpp"

#import "CSMachine+Subclassing.h"
#import "NSData+StdVector.h"
#import "NSBundle+DataResource.h"

@implementation CSOric {
	Oric::Machine _oric;
}

- (instancetype)init {
	self = [super init];
	if(self)
	{
		NSData *rom = [self rom:@"basic10"];
		if(rom) _oric.set_rom(rom.stdVector8);
	}
	return self;
}

- (NSData *)rom:(NSString *)name
{
	return [[NSBundle mainBundle] dataForResource:name withExtension:@"rom" subdirectory:@"ROMImages/Oric"];
}

- (CRTMachine::Machine * const)machine {
	return &_oric;
}

@end
