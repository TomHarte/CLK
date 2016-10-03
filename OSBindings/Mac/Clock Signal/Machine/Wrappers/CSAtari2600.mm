//
//  Atari2600.m
//  CLK
//
//  Created by Thomas Harte on 14/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

#import "CSAtari2600.h"

#include "Atari2600.hpp"
#import "CSMachine+Subclassing.h"

@interface CSAtari2600 ()
- (void)crt:(Outputs::CRT::CRT *)crt didEndBatchOfFrames:(unsigned int)numberOfFrames withUnexpectedVerticalSyncs:(unsigned int)numberOfUnexpectedSyncs;
@end

struct CRTDelegate: public Outputs::CRT::Delegate {
	__weak CSAtari2600 *atari2600;
	void crt_did_end_batch_of_frames(Outputs::CRT::CRT *crt, unsigned int number_of_frames, unsigned int number_of_unexpected_vertical_syncs) {
		[atari2600 crt:crt didEndBatchOfFrames:number_of_frames withUnexpectedVerticalSyncs:number_of_unexpected_vertical_syncs];
	}
};

@implementation CSAtari2600 {
	Atari2600::Machine _atari2600;
	CRTDelegate _crtDelegate;

	int _frameCount;
	int _hitCount;
	BOOL _didDecideRegion;
	int _batchesReceived;
}

- (void)crt:(Outputs::CRT::CRT *)crt didEndBatchOfFrames:(unsigned int)numberOfFrames withUnexpectedVerticalSyncs:(unsigned int)numberOfUnexpectedSyncs {
	if(!_didDecideRegion)
	{
		_batchesReceived++;
		if(_batchesReceived == 2)
		{
			_didDecideRegion = YES;
			if(numberOfUnexpectedSyncs >= numberOfFrames >> 1)
			{
				[self.view performWithGLContext:^{
					_atari2600.switch_region();
				}];
			}
		}
	}
}

- (void)setDirection:(CSJoystickDirection)direction onPad:(NSUInteger)pad isPressed:(BOOL)isPressed {
	Atari2600DigitalInput input;
	switch(direction)
	{
		case CSJoystickDirectionUp:		input = pad ? Atari2600DigitalInputJoy2Up : Atari2600DigitalInputJoy1Up;		break;
		case CSJoystickDirectionDown:	input = pad ? Atari2600DigitalInputJoy2Down : Atari2600DigitalInputJoy1Down;	break;
		case CSJoystickDirectionLeft:	input = pad ? Atari2600DigitalInputJoy2Left : Atari2600DigitalInputJoy1Left;	break;
		case CSJoystickDirectionRight:	input = pad ? Atari2600DigitalInputJoy2Right : Atari2600DigitalInputJoy1Right;	break;
	}
	@synchronized(self) {
		_atari2600.set_digital_input(input, isPressed ? true : false);
	}
}

- (void)setButtonAtIndex:(NSUInteger)button onPad:(NSUInteger)pad isPressed:(BOOL)isPressed {
	@synchronized(self) {
		_atari2600.set_digital_input(pad ? Atari2600DigitalInputJoy2Fire : Atari2600DigitalInputJoy1Fire, isPressed ? true : false);
	}
}

- (void)setResetLineEnabled:(BOOL)enabled {
	@synchronized(self) {
		_atari2600.set_reset_line(enabled ? true : false);
	}
}

- (void)setupOutputWithAspectRatio:(float)aspectRatio {
	@synchronized(self) {
		[super setupOutputWithAspectRatio:aspectRatio];
		_atari2600.get_crt()->set_delegate(&_crtDelegate);
		_crtDelegate.atari2600 = self;
	}
}

- (CRTMachine::Machine * const)machine {
	return &_atari2600;
}

#pragma mark - Switches

- (void)setColourButton:(BOOL)colourButton {
	_colourButton = colourButton;
	@synchronized(self) {
		_atari2600.set_switch_is_enabled(Atari2600SwitchColour, colourButton);
	}
}

- (void)setLeftPlayerDifficultyButton:(BOOL)leftPlayerDifficultyButton {
	_leftPlayerDifficultyButton = leftPlayerDifficultyButton;
	@synchronized(self) {
		_atari2600.set_switch_is_enabled(Atari2600SwitchLeftPlayerDifficulty, leftPlayerDifficultyButton);
	}
}

- (void)setRightPlayerDifficultyButton:(BOOL)rightPlayerDifficultyButton {
	_rightPlayerDifficultyButton = rightPlayerDifficultyButton;
	@synchronized(self) {
		_atari2600.set_switch_is_enabled(Atari2600SwitchRightPlayerDifficulty, rightPlayerDifficultyButton);
	}
}

- (void)toggleSwitch:(Atari2600Switch)toggleSwitch {
	@synchronized(self) {
		_atari2600.set_switch_is_enabled(toggleSwitch, true);
	}
	dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.5 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
		@synchronized(self) {
			_atari2600.set_switch_is_enabled(toggleSwitch, false);
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
