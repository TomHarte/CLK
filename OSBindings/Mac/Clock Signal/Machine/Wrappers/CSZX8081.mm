//
//  CSZX8081.m
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#import "CSZX8081.h"

#include "ZX8081.hpp"
#include "StandardOptions.hpp"

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

		Configurable::SelectionSet selection_set;
		append_quick_load_tape_selection(selection_set, useFastLoadingHack ? true : false);
		_zx8081->set_selections(selection_set);
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

		Configurable::SelectionSet selection_set;
		append_automatic_tape_motor_control_selection(selection_set, useAutomaticTapeMotorControl ? true : false);
		_zx8081->set_selections(selection_set);
	}
}

@end
