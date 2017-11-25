//
//  Atari2600.m
//  CLK
//
//  Created by Thomas Harte on 14/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

#import "CSAtari2600.h"

#include "Atari2600.hpp"
#include "TypedDynamicMachine.hpp"
#import "CSMachine+Subclassing.h"

@implementation CSAtari2600 {
	Machine::TypedDynamicMachine<Atari2600::Machine> _atari2600;
}

- (instancetype)init {
	_atari2600 = Machine::TypedDynamicMachine<Atari2600::Machine>(Atari2600::Machine::Atari2600());
	return [super initWithMachine:&_atari2600];
}

- (void)setResetLineEnabled:(BOOL)enabled {
	@synchronized(self) {
		_atari2600.get()->set_reset_switch(enabled ? true : false);
	}
}

- (void)setupOutputWithAspectRatio:(float)aspectRatio {
	@synchronized(self) {
		[super setupOutputWithAspectRatio:aspectRatio];
	}
}

#pragma mark - Switches

- (void)setColourButton:(BOOL)colourButton {
	_colourButton = colourButton;
	@synchronized(self) {
		_atari2600.get()->set_switch_is_enabled(Atari2600SwitchColour, colourButton);
	}
}

- (void)setLeftPlayerDifficultyButton:(BOOL)leftPlayerDifficultyButton {
	_leftPlayerDifficultyButton = leftPlayerDifficultyButton;
	@synchronized(self) {
		_atari2600.get()->set_switch_is_enabled(Atari2600SwitchLeftPlayerDifficulty, leftPlayerDifficultyButton);
	}
}

- (void)setRightPlayerDifficultyButton:(BOOL)rightPlayerDifficultyButton {
	_rightPlayerDifficultyButton = rightPlayerDifficultyButton;
	@synchronized(self) {
		_atari2600.get()->set_switch_is_enabled(Atari2600SwitchRightPlayerDifficulty, rightPlayerDifficultyButton);
	}
}

- (void)toggleSwitch:(Atari2600Switch)toggleSwitch {
	@synchronized(self) {
		_atari2600.get()->set_switch_is_enabled(toggleSwitch, true);
	}
	dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.5 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
		@synchronized(self) {
			_atari2600.get()->set_switch_is_enabled(toggleSwitch, false);
		}
	});
}

- (void)pressResetButton {
	[self toggleSwitch:Atari2600SwitchReset];
}

- (void)pressSelectButton {
	[self toggleSwitch:Atari2600SwitchSelect];
}

- (NSString *)userDefaultsPrefix {	return @"atari2600";	}

@end
