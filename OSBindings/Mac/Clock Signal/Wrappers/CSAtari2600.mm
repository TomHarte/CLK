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
}

- (void)crt:(Outputs::CRT::CRT *)crt didEndBatchOfFrames:(unsigned int)numberOfFrames withUnexpectedVerticalSyncs:(unsigned int)numberOfUnexpectedSyncs {
	if(!_didDecideRegion)
	{
		_didDecideRegion = YES;
		if(numberOfUnexpectedSyncs >= numberOfFrames >> 1)
		{
			_atari2600.switch_region();
		}
	}
}

- (void)runForNumberOfCycles:(int)numberOfCycles {
	@synchronized(self) {
		_atari2600.run_for_cycles(numberOfCycles);
	}
}

- (void)drawViewForPixelSize:(CGSize)pixelSize onlyIfDirty:(BOOL)onlyIfDirty {
	_atari2600.get_crt()->draw_frame((unsigned int)pixelSize.width, (unsigned int)pixelSize.height, onlyIfDirty ? true : false);
}

- (void)setROM:(NSData *)rom {
	@synchronized(self) {
		_atari2600.set_rom(rom.length, (const uint8_t *)rom.bytes);
	}
}

- (void)setState:(BOOL)state forDigitalInput:(Atari2600DigitalInput)digitalInput {
	@synchronized(self) {
		_atari2600.set_digital_input(digitalInput, state ? true : false);
	}
}

- (void)setResetLineEnabled:(BOOL)enabled {
	@synchronized(self) {
		_atari2600.set_reset_line(enabled ? true : false);
	}
}

- (void)setupOutputWithAspectRatio:(float)aspectRatio {
	@synchronized(self) {
		_atari2600.setup_output(aspectRatio);
		_atari2600.get_crt()->set_delegate(&_crtDelegate);
		_crtDelegate.atari2600 = self;
	}
}

@end
