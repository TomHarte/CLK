//
//  CSOric.m
//  Clock Signal
//
//  Created by Thomas Harte on 11/10/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import "CSOric.h"

#include "Oric.hpp"
#include "StandardOptions.hpp"

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

		Configurable::SelectionSet selection_set;
		append_quick_load_tape_selection(selection_set, useFastLoadingHack ? true : false);
		_oric->set_selections(selection_set);
	}
}

- (void)setUseCompositeOutput:(BOOL)useCompositeOutput {
	@synchronized(self) {
		_useCompositeOutput = useCompositeOutput;

		Configurable::SelectionSet selection_set;
		append_display_selection(selection_set, useCompositeOutput ? Configurable::Display::Composite : Configurable::Display::RGB);
		_oric->set_selections(selection_set);
	}
}

@end
