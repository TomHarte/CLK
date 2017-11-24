//
//  CSElectron.m
//  Clock Signal
//
//  Created by Thomas Harte on 04/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import "CSElectron.h"

#include "Electron.hpp"
#include "StandardOptions.hpp"

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

		Configurable::SelectionSet selection_set;
		append_quick_load_tape_selection(selection_set, useFastLoadingHack ? true : false);
		_electron->set_selections(selection_set);
	}
}

- (void)setUseTelevisionOutput:(BOOL)useTelevisionOutput {
	@synchronized(self) {
		_useTelevisionOutput = useTelevisionOutput;

		Configurable::SelectionSet selection_set;
		append_display_selection(selection_set, useTelevisionOutput ? Configurable::Display::Composite : Configurable::Display::RGB);
		_electron->set_selections(selection_set);
	}
}
@end
