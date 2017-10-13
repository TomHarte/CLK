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
	std::unique_ptr<Oric::Machine> _oric;
}

- (instancetype)init {
	Oric::Machine *machine = Oric::Machine::Oric();

	self = [super initWithMachine:machine];
	if(self) {
		_oric.reset(machine);

		NSData *basic10 = [self rom:@"basic10"];
		NSData *basic11 = [self rom:@"basic11"];
		NSData *colour = [self rom:@"colour"];
		NSData *microdisc = [self rom:@"microdisc"];

		if(basic10)		_oric->set_rom(Oric::BASIC10, basic10.stdVector8);
		if(basic11)		_oric->set_rom(Oric::BASIC11, basic11.stdVector8);
		if(colour)		_oric->set_rom(Oric::Colour, colour.stdVector8);
		if(microdisc)	_oric->set_rom(Oric::Microdisc, microdisc.stdVector8);
	}
	return self;
}

- (NSData *)rom:(NSString *)name {
	return [[NSBundle mainBundle] dataForResource:name withExtension:@"rom" subdirectory:@"ROMImages/Oric"];
}

#pragma mark - Options

- (void)setUseFastLoadingHack:(BOOL)useFastLoadingHack {
	@synchronized(self) {
		_useFastLoadingHack = useFastLoadingHack;
		_oric->set_use_fast_tape_hack(useFastLoadingHack ? true : false);
	}
}

- (void)setUseCompositeOutput:(BOOL)useCompositeOutput {
	@synchronized(self) {
		_useCompositeOutput = useCompositeOutput;
		_oric->set_output_device(useCompositeOutput ? Outputs::CRT::Television : Outputs::CRT::Monitor);
	}
}

@end
