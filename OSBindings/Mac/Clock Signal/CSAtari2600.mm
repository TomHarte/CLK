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

typedef NS_ENUM(NSInteger, CSAtari2600RunningState) {
	CSAtari2600RunningStateRunning,
	CSAtari2600RunningStateStopped
};

@implementation CSAtari2600 {
	Atari2600::Machine _atari2600;
	Atari2600CRTDelegate _crtDelegate;

	dispatch_queue_t _serialDispatchQueue;

	int _frameCount;
	int _hitCount;
	BOOL _didDecideRegion;

	NSConditionLock *_runningLock;
}

- (void)crtDidEndFrame:(CRTFrame *)frame didDetectVSync:(BOOL)didDetectVSync {

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

	BOOL hasReturn = [self.view pushFrame:frame];

	if(hasReturn)
		_atari2600.get_crt()->return_frame();
}

- (void)runForNumberOfCycles:(int)cycles {
	if([_runningLock tryLockWhenCondition:CSAtari2600RunningStateStopped]) {
		[_runningLock unlockWithCondition:CSAtari2600RunningStateRunning];
		dispatch_async(_serialDispatchQueue, ^{
			[_runningLock lockWhenCondition:CSAtari2600RunningStateRunning];
			_atari2600.run_for_cycles(cycles);
			[_runningLock unlockWithCondition:CSAtari2600RunningStateStopped];
		});
	}
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

- (void)setView:(CSCathodeRayView *)view {
	_view = view;
	_view.signalDecoder = [NSString stringWithUTF8String:_atari2600.get_signal_decoder()];
}

- (instancetype)init {
	self = [super init];

	if (self) {
		_crtDelegate.atari = self;
		_atari2600.get_crt()->set_delegate(&_crtDelegate);
		_serialDispatchQueue = dispatch_queue_create("Atari 2600 queue", DISPATCH_QUEUE_SERIAL);
		_runningLock = [[NSConditionLock alloc] initWithCondition:CSAtari2600RunningStateStopped];
	}

	return self;
}

@end
