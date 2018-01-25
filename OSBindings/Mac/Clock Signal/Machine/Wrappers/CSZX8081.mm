//
//  CSZX8081.m
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#import "CSZX8081.h"

#include "ZX8081.hpp"
#include "TypedDynamicMachine.hpp"

@implementation CSZX8081 {
	Machine::TypedDynamicMachine<ZX8081::Machine> _zx8081;
}

- (instancetype)initWithIntendedTarget:(const StaticAnalyser::Target &)target {
	_zx8081 = Machine::TypedDynamicMachine<ZX8081::Machine>(ZX8081::Machine::ZX8081(target));
	return nil;//[super initWithMachine:&_zx8081];
}

- (NSString *)userDefaultsPrefix {	return @"zx8081";	}

#pragma mark - Options

- (void)setTapeIsPlaying:(BOOL)tapeIsPlaying {
	@synchronized(self) {
		_tapeIsPlaying = tapeIsPlaying;
		_zx8081.get()->set_tape_is_playing(tapeIsPlaying ? true : false);
	}
}

@end
