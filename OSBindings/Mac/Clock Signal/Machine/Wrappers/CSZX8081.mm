//
//  CSZX8081.m
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#import "CSZX8081.h"

#include "ZX8081.hpp"

@implementation CSZX8081 {
	Sinclair::ZX8081::Machine *_zx8081;
	__weak CSMachine *_machine;
}

- (instancetype)initWithZX8081:(void *)zx8081 owner:(CSMachine *)machine {
	self = [super init];
	if(self) {
		_zx8081 = (Sinclair::ZX8081::Machine *)zx8081;
		_machine = machine;
	}
	return self;
}

#pragma mark - Options

- (void)setTapeIsPlaying:(BOOL)tapeIsPlaying {
	@synchronized(_machine) {
		_zx8081->set_tape_is_playing(tapeIsPlaying ? true : false);
	}
}

- (BOOL)tapeIsPlaying {
	@synchronized(_machine) {
		return _zx8081->get_tape_is_playing();
	}
}

@end
