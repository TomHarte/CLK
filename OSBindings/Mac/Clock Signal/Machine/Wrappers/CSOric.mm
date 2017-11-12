//
//  CSOric.m
//  Clock Signal
//
//  Created by Thomas Harte on 11/10/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import "CSOric.h"

#include "Oric.hpp"

@implementation CSOric {
	std::unique_ptr<Oric::Machine> _oric;
}

- (instancetype)init {
	Oric::Machine *machine = Oric::Machine::Oric();

	self = [super initWithMachine:machine];
	if(self) {
		_oric.reset(machine);
	}
	return self;
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
		_oric->set_output_device(useCompositeOutput ? Outputs::CRT::OutputDevice::Television : Outputs::CRT::OutputDevice::Monitor);
	}
}

@end
