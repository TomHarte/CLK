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

	int _failedVSyncCount;
}

- (void)crtDidEndFrame:(CRTFrame *)frame didDetectVSync:(BOOL)didDetectVSync {

	if(!didDetectVSync)
	{
		_failedVSyncCount+=2;

		if(_failedVSyncCount == 60)
		{
			_atari2600.switch_region();
		}
	}
	else
	{
		_failedVSyncCount = MAX(_failedVSyncCount - 1, 0);
	}

	dispatch_async(dispatch_get_main_queue(), ^{
		BOOL hasReturn = [self.view pushFrame:frame];

		if(hasReturn)
			dispatch_async(_serialDispatchQueue, ^{
				_atari2600.get_crt()->return_frame();
			});
	});
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
