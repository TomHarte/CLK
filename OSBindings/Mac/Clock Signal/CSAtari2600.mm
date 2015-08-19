//
//  Atari2600.m
//  CLK
//
//  Created by Thomas Harte on 14/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

#import "CSAtari2600.h"
#import "Atari2600.hpp"

@interface CSAtari2600 (Callbacks)
- (void)crtDidEndFrame:(CRTFrame *)frame didDetectVSync:(BOOL)didDetectVSync;
@end

struct Atari2600CRTDelegate: public Outputs::CRT::CRTDelegate {
	__weak CSAtari2600 *atari;
	void crt_did_end_frame(Outputs::CRT *crt, CRTFrame *frame, bool did_detect_vsync) { [atari crtDidEndFrame:frame didDetectVSync:did_detect_vsync]; }
};

@implementation CSAtari2600 {
	Atari2600::Machine _atari2600;
	Atari2600CRTDelegate _crtDelegate;

	dispatch_queue_t _serialDispatchQueue;

	int _frameCount;
	NSTimeInterval _startTimeInterval;
	BOOL _didDecideRegion;
}

- (void)crtDidEndFrame:(CRTFrame *)frame didDetectVSync:(BOOL)didDetectVSync {

	if(!_didDecideRegion)
	{
		if(_startTimeInterval < 1.0)
		{
			_startTimeInterval = [NSDate timeIntervalSinceReferenceDate];
		}
		else
		{
			_frameCount++;

			if(_frameCount > 30)
			{
				float fps = (float)_frameCount / (float)([NSDate timeIntervalSinceReferenceDate] - _startTimeInterval);

				if(fabsf(fps - 50.0f) < 5.0f)
				{
					_atari2600.switch_region();
					_didDecideRegion = YES;
				}

				if(fabsf(fps - 60.0f) < 5.0f)
				{
					_didDecideRegion = YES;
				}
			}
		}
	}

	BOOL hasReturn = [self.view pushFrame:frame];

	if(hasReturn)
		_atari2600.get_crt()->return_frame();
}

- (void)runForNumberOfCycles:(int)cycles {
	dispatch_async(_serialDispatchQueue, ^{
		_atari2600.run_for_cycles(cycles);
	});
}

- (void)setROM:(NSData *)rom {
	dispatch_async(_serialDispatchQueue, ^{
		_atari2600.set_rom(rom.length, (const uint8_t *)rom.bytes);
	});
}

- (void)setState:(BOOL)state forDigitalInput:(Atari2600DigitalInput)digitalInput {
	dispatch_async(_serialDispatchQueue, ^{
		_atari2600.set_digital_input(digitalInput, state ? true : false);
	});
}

- (void)setResetLineEnabled:(BOOL)enabled {
	dispatch_async(_serialDispatchQueue, ^{
		_atari2600.set_reset_line(enabled ? true : false);
	});
}

- (instancetype)init {
	self = [super init];

	if (self) {
		_crtDelegate.atari = self;
		_atari2600.get_crt()->set_delegate(&_crtDelegate);
		_serialDispatchQueue = dispatch_queue_create("Atari 2600 queue", DISPATCH_QUEUE_SERIAL);
	}

	return self;
}

@end
