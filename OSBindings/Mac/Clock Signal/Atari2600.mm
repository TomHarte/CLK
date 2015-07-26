//
//  Atari2600.m
//  CLK
//
//  Created by Thomas Harte on 14/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

#import "Atari2600.h"
#import "Atari2600.hpp"

@interface CSAtari2600 (Callbacks)
- (void)crtDidEndFrame:(CRTFrame *)frame;
@end

struct Atari2600CRTDelegate: public Outputs::CRT::CRTDelegate {
	CSAtari2600 *atari;
	void crt_did_end_frame(Outputs::CRT *crt, CRTFrame *frame) { [atari crtDidEndFrame:frame]; }
};

@implementation CSAtari2600 {
	Atari2600::Machine _atari2600;
	Atari2600CRTDelegate _crtDelegate;

	dispatch_queue_t _serialDispatchQueue;
}

- (void)crtDidEndFrame:(CRTFrame *)frame {

	dispatch_async(dispatch_get_main_queue(), ^{
		BOOL hasReturn = !!self.view.crtFrame;
		self.view.crtFrame = frame;

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
