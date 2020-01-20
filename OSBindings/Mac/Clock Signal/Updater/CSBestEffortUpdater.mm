//
//  CSBestEffortUpdater.m
//  Clock Signal
//
//  Created by Thomas Harte on 16/06/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#import "CSBestEffortUpdater.h"

#include "BestEffortUpdater.hpp"

struct UpdaterDelegate: public Concurrency::BestEffortUpdater::Delegate {
	__weak CSMachine *machine;

	void update(Concurrency::BestEffortUpdater *updater, Time::Seconds seconds, bool did_skip_previous_update, int flags) {
		[machine runForInterval:seconds];
	}
};

@implementation CSBestEffortUpdater {
	Concurrency::BestEffortUpdater _updater;
	UpdaterDelegate _updaterDelegate;
}

- (instancetype)init {
	self = [super init];
	if(self) {
		_updater.set_delegate(&_updaterDelegate);
	}
	return self;
}

- (void)update {
	_updater.update();
}

- (void)flush {
	_updater.flush();
}

- (void)setMachine:(CSMachine *)machine {
	_updater.flush();
	_updaterDelegate.machine = machine;
}

@end
