//
//  Atari2600.m
//  ElectrEm
//
//  Created by Thomas Harte on 14/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

#import "Atari2600.h"
#import "Atari2600.hpp"
#import <OpenGL/gl.h>

@interface CSAtari2600 (Callbacks)
- (void)crtDidEndFrame:(Outputs::CRTFrame *)frame;
@end

struct Atari2600CRTDelegate: public Outputs::CRT::CRTDelegate {
	CSAtari2600 *atari;
	void crt_did_end_frame(Outputs::CRT *crt, Outputs::CRTFrame *frame) { [atari crtDidEndFrame:frame]; }
};

@implementation CSAtari2600 {
	Atari2600::Machine _atari2600;
	Atari2600CRTDelegate _crtDelegate;

	dispatch_queue_t _serialDispatchQueue;
	Outputs::CRTFrame *_queuedFrame;
}

- (void)crtDidEndFrame:(Outputs::CRTFrame *)frame {

	dispatch_async(dispatch_get_main_queue(), ^{
		if(_queuedFrame) {
			dispatch_async(_serialDispatchQueue, ^{
				_atari2600.get_crt()->return_frame();
			});
		}
		_queuedFrame = frame;

		[self.delegate atari2600NeedsRedraw:self];
	});
}

- (void)draw {

	if(_queuedFrame)
	{
		glClearColor(1.0, 0.0, 0.0, 1.0);
		glClear(GL_COLOR_BUFFER_BIT);
/*		printf("\n\n===\n\n");
		int c = 0;
		for(int run = 0; run < _queuedFrame->number_of_runs; run++)
		{
			char character = ' ';
			switch(_queuedFrame->runs[run].type)
			{
				case Outputs::CRTRun::Type::Sync:	character = '<'; break;
				case Outputs::CRTRun::Type::Level:	character = '_'; break;
				case Outputs::CRTRun::Type::Data:	character = '-'; break;
				case Outputs::CRTRun::Type::Blank:	character = ' '; break;
			}

			if(_queuedFrame->runs[run].start_point.dst_x < 1.0 / 224.0)
			{
				printf("\n[%0.2f]: ", _queuedFrame->runs[run].start_point.dst_y);
				c++;
			}

			printf("(%0.2f): ", _queuedFrame->runs[run].start_point.dst_x);
			float length = fabsf(_queuedFrame->runs[run].end_point.dst_x - _queuedFrame->runs[run].start_point.dst_x);
			int iLength = (int)(length * 64.0);
			for(int c = 0; c < iLength; c++)
			{
				putc(character, stdout);
			}
		}

		printf("\n\n[%d]\n\n", c);*/
	}
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
