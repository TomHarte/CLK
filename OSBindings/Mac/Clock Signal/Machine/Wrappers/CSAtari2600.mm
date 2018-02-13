//
//  Atari2600.m
//  CLK
//
//  Created by Thomas Harte on 14/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

#import "CSAtari2600.h"

#include "Atari2600.hpp"

@implementation CSAtari2600 {
	Atari2600::Machine *_atari2600;
	__weak CSMachine *_machine;
}

- (instancetype)initWithAtari2600:(void *)atari2600 owner:(CSMachine *)machine {
	self = [super init];
	if(self) {
		_atari2600 = (Atari2600::Machine *)atari2600;
		_machine = machine;
	}
	return self;
}

- (void)setColourButton:(BOOL)colourButton {
	_colourButton = colourButton;

	@synchronized(_machine) {
		_atari2600->set_switch_is_enabled(Atari2600SwitchColour, colourButton);
	}
}

- (void)setLeftPlayerDifficultyButton:(BOOL)leftPlayerDifficultyButton {
	_leftPlayerDifficultyButton = leftPlayerDifficultyButton;

	@synchronized(_machine) {
		_atari2600->set_switch_is_enabled(Atari2600SwitchLeftPlayerDifficulty, leftPlayerDifficultyButton);
	}
}

- (void)setRightPlayerDifficultyButton:(BOOL)rightPlayerDifficultyButton {
	_rightPlayerDifficultyButton = rightPlayerDifficultyButton;

	@synchronized(_machine) {
		_atari2600->set_switch_is_enabled(Atari2600SwitchRightPlayerDifficulty, rightPlayerDifficultyButton);
	}
}

- (void)toggleSwitch:(Atari2600Switch)toggleSwitch {
	@synchronized(_machine) {
		_atari2600->set_switch_is_enabled(toggleSwitch, true);
	}

	dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.5 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
		@synchronized(_machine) {
			_atari2600->set_switch_is_enabled(toggleSwitch, false);
		}
	});
}

- (void)pressResetButton {
	[self toggleSwitch:Atari2600SwitchReset];
}

- (void)pressSelectButton {
	[self toggleSwitch:Atari2600SwitchSelect];
}

@end
