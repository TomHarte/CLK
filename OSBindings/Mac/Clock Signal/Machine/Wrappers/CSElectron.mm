//
//  CSElectron.m
//  Clock Signal
//
//  Created by Thomas Harte on 04/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import "CSElectron.h"

#include "Electron.hpp"


@implementation CSElectron {
	std::unique_ptr<Electron::Machine> _electron;
}

- (instancetype)init {
	Electron::Machine *machine = Electron::Machine::Electron();

	self = [super initWithMachine:machine];
	if(self) {
		_electron.reset(machine);
	}
	return self;
}

#pragma mark - ROM setting

- (NSString *)userDefaultsPrefix {	return @"electron";	}

#pragma mark - Options

- (void)setUseFastLoadingHack:(BOOL)useFastLoadingHack {
	@synchronized(self) {
		_useFastLoadingHack = useFastLoadingHack;
		_electron->set_use_fast_tape_hack(useFastLoadingHack ? true : false);
	}
}

- (void)setUseTelevisionOutput:(BOOL)useTelevisionOutput {
	@synchronized(self) {
		_useTelevisionOutput = useTelevisionOutput;
		_electron->get_crt()->set_output_device(useTelevisionOutput ? Outputs::CRT::Television : Outputs::CRT::Monitor);
	}
}
@end
