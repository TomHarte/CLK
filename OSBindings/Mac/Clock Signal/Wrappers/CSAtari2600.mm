//
//  Atari2600.m
//  CLK
//
//  Created by Thomas Harte on 14/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

#import "CSAtari2600.h"

#import "Atari2600.hpp"
#import "CSMachine+Subclassing.h"

@implementation CSAtari2600 {
	Atari2600::Machine _atari2600;

	int _frameCount;
	int _hitCount;
	BOOL _didDecideRegion;
}

/*- (void)crt:(Outputs::CRT *)crt didEndFrame:(CRTFrame *)frame didDetectVSync:(BOOL)didDetectVSync {
	if(!_didDecideRegion)
	{
		_frameCount++;
		_hitCount += didDetectVSync ? 1 : 0;

		if(_frameCount > 30)
		{
			if(_hitCount < _frameCount >> 1)
			{
				_atari2600.switch_region();
				_didDecideRegion = YES;
			}

			if(_hitCount > (_frameCount * 7) >> 3)
			{
				_didDecideRegion = YES;
			}
		}
	}

	[super crt:crt didEndFrame:frame didDetectVSync:didDetectVSync];
}*/

- (void)doRunForNumberOfCycles:(int)numberOfCycles {
	_atari2600.run_for_cycles(numberOfCycles);
}

- (void)drawViewForPixelSize:(CGSize)pixelSize onlyIfDirty:(BOOL)onlyIfDirty {
	_atari2600.get_crt()->draw_frame((unsigned int)pixelSize.width, (unsigned int)pixelSize.height, onlyIfDirty ? true : false);
}

- (void)setROM:(NSData *)rom {
		_atari2600.set_rom(rom.length, (const uint8_t *)rom.bytes);
}

- (void)setState:(BOOL)state forDigitalInput:(Atari2600DigitalInput)digitalInput {
		_atari2600.set_digital_input(digitalInput, state ? true : false);
}

- (void)setResetLineEnabled:(BOOL)enabled {
		_atari2600.set_reset_line(enabled ? true : false);
}

@end
