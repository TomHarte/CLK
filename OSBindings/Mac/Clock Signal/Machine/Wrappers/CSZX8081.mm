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
	std::unique_ptr<ZX8081::Machine> _zx8081;
}

- (instancetype)initWithIntendedTarget:(const StaticAnalyser::Target &)target {
	ZX8081::Machine *machine = ZX8081::Machine::ZX8081(target);

	self = [super initWithMachine:machine];
	if(self) {
		_zx8081.reset(machine);
	}
	return self;
}

- (NSString *)userDefaultsPrefix {	return @"zx8081";	}

#pragma mark - Options

- (void)setUseFastLoadingHack:(BOOL)useFastLoadingHack {
	@synchronized(self) {
		_useFastLoadingHack = useFastLoadingHack;
		_zx8081->set_use_fast_tape_hack(useFastLoadingHack ? true : false);
	}
}

- (void)setTapeIsPlaying:(BOOL)tapeIsPlaying {
	@synchronized(self) {
		_tapeIsPlaying = tapeIsPlaying;
		_zx8081->set_tape_is_playing(tapeIsPlaying ? true : false);
	}
}

- (void)setUseAutomaticTapeMotorControl:(BOOL)useAutomaticTapeMotorControl {
	@synchronized(self) {
		_useAutomaticTapeMotorControl = useAutomaticTapeMotorControl;
		_zx8081->set_use_automatic_tape_motor_control(useAutomaticTapeMotorControl ? true : false);
	}
}

@end
